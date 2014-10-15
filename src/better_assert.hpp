#ifndef BETTER_ASSERT_HPP
#define BETTER_ASSERT_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <type_traits>

namespace _assert_defaults {
    template <typename T,
        typename = std::enable_if_t<
            !std::is_same<std::decay_t<T>,char const*>::value,
            void>>
    std::ostream& operator<<(std::ostream& out, T&& t) {
        out << "???";
        return out;
    }
}

template <typename B>
struct _asserter {
    const char* expr;
    const char* fname;
    int line;
    B success;
    std::ostringstream ss;

    _asserter(const char* expr, const char* filename, int line, B b)
        : expr(expr), fname(filename), line(line), success(b), ss{} {}

    template <typename T>
    _asserter& operator->*(T const& t) {
        using namespace _assert_defaults;
        ss << t;
        return *this;
    }

#define opdef(op) \
    template <typename T> \
    _asserter& operator op (T const& t) { \
        using namespace _assert_defaults; \
        ss << " " #op " " << t; \
        return *this; \
    }

    opdef(&&)
    opdef(||)
    opdef(==)
    opdef(!=)
    opdef(<)
    opdef(>)
    opdef(<=)
    opdef(>=)
    opdef(+)
    opdef(-)
    opdef(*)
    opdef(/)
    opdef(%)
    opdef(|)
    opdef(&)
    opdef(^)

    template <typename T>
    _asserter& operator , (T const& t) {
        using namespace _assert_defaults;
        ss << " , " << t;
        return *this;
    }

#undef opdef

    void check() const {
        if (!success) {
            using std::cerr;
            using std::endl;
            cerr << endl;
            cerr << "###!!!========================== ASSERT FAIL ==========================!!!###" << endl;
            cerr << fname << " : " << line << endl;
            cerr << "( " << expr << " )" << endl;
            cerr << "( " << ss.str() << " )" << endl;
            cerr << "( " << success << " )" << endl;
            cerr << endl;
            throw std::logic_error("Assertion failed.");
        }
    }
};

// Disable normal assert
#include <cassert>
#undef assert

#ifndef BETTER_ASSERT_OFF

    #define assert(...) ((_asserter<decltype(__VA_ARGS__)>(#__VA_ARGS__,__FILE__,__LINE__,(__VA_ARGS__)) ->* __VA_ARGS__).check())
    #define assert_if(cond) if (cond)

#else

    #define assert(cond) ((void)0)
    #define assert_if(cond) if (false)

#endif

#endif // BETTER_ASSERT_HPP
