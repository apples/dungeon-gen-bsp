#ifndef ARRAY_VECTOR_HPP
#define ARRAY_VECTOR_HPP

#include "better_assert.hpp"

#include <cstddef>
#include <utility>

template <typename T, std::size_t N>
class ArrayVector {
    alignas(T) char data[sizeof(T)*N];
    std::size_t sz = 0;

public:

    using value_type = T;

    ArrayVector() = default;

    ArrayVector(ArrayVector const& other) {
        for (auto& x : other) {
            push_back(x);
        }
    }

    ArrayVector(ArrayVector&& other) {
        for (auto& x : other) {
            push_back(std::move(x));
        }
        other.clear();
    }

    ArrayVector& operator=(ArrayVector const& other) {
        clear();
        for (auto& x : other) {
            push_back(x);
        }
        return *this;
    }

    ArrayVector& operator=(ArrayVector&& other) {
        clear();
        for (auto& x : other) {
            push_back(std::move(x));
        }
        other.clear();
        return *this;
    }

    ~ArrayVector() {
        clear();
    }

    T* begin() {
        ArrayVector const& self = *this;
        return const_cast<T*>(self.begin());
    }

    T* end() {
        ArrayVector const& self = *this;
        return const_cast<T*>(self.end());
    }

    T const* begin() const {
        return reinterpret_cast<T const*>(&data[0]);
    }

    T const* end() const {
        return reinterpret_cast<T const*>(&data[0]) + sz;
    }

    std::size_t size() const {
        return sz;
    }

    void push_back(T t) {
        assert(sz < N);
        new (end()) T(std::move(t));
        ++sz;
    }

    void pop_back() {
        assert(sz > 0);
        --sz;
        end()->~T();
    }

    void clear() {
        while (sz > 0) {
            pop_back();
        }
    }
};

#endif
