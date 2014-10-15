#ifndef THREAD_WORKER_HPP
#define THREAD_WORKER_HPP

#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <list>
#include <utility>
#include <functional>
#include <exception>
#include <vector>
#include <memory>

template <typename Data>
struct ThreadWorker {
    using Task = std::pair<std::function<Data()>,std::promise<Data>>;

    struct Worker {
        std::thread t;

        struct MTCV {
            std::mutex mt;
            std::condition_variable cv;
        };

        std::unique_ptr<MTCV> mtcv = std::make_unique<MTCV>();

        bool die = false;
        std::list<Task> tasks;

        Worker() {
            t = std::thread(&Worker::loop, this);
        }

        void loop() {
            std::unique_lock<std::mutex> lk (mtcv->mt);
            while (true) {
                while (tasks.empty() && !die)
                    mtcv->cv.wait(lk);

                while (!tasks.empty()) {
                    auto task = std::move(tasks.front());
                    tasks.erase(tasks.begin());
                    lk.unlock();
                    try {
                        task.second.set_value(task.first());
                    } catch (...) {
                        task.second.set_exception(std::current_exception());
                    }
                    lk.lock();
                }

                if (die) {
                    return;
                }
            }
        }
    };

    std::vector<Worker> workers;

    ThreadWorker() {
        workers.resize(std::min(std::thread::hardware_concurrency(),1u));
    }

    ~ThreadWorker() {
        for (auto& w : workers) {
            {
                std::unique_lock<std::mutex> lk (w.mtcv->mt);
                w.die = true;
            }
            w.mtcv->cv.notify_one();
            w.t.join();
        }
    }

    template <typename Func, typename... Ts>
    std::future<Data> do_task(Func&& f, Ts&&... ts) {
        auto iter = min_element(begin(workers),end(workers),[](auto& w1, auto& w2){
            std::unique_lock<std::mutex> lk1 (w1.mtcv->mt, std::defer_lock);
            std::unique_lock<std::mutex> lk2 (w2.mtcv->mt, std::defer_lock);
            std::lock(lk1,lk2);
            return w1.tasks.size()<w2.tasks.size();
        });
        std::list<Task> tmp;
        // TODO: Perfect forwarding?
        auto func = [=]{ return std::move(f)(std::move(ts)...); };
        auto task = std::make_pair(std::move(func), std::promise<Data>{});
        auto rv = task.second.get_future();
        tmp.push_back(std::move(task));
        {
            std::unique_lock<std::mutex> lk (iter->mtcv->mt);
            iter->tasks.splice(iter->tasks.end(), tmp);
        }
        iter->mtcv->cv.notify_one();
        return rv;
    }
};

#endif
