#ifndef BETTER_ASSERT_HPP
#define BETTER_ASSERT_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

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
        ss << t;
        return *this;
    }

#define opdef(op) \
    template <typename T> \
    _asserter& operator op (T const& t) { \
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
    //TODO: opdef(,) ?

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
#define assert(cond) ((_asserter<decltype(cond)>(#cond,__FILE__,__LINE__,(cond)) ->* cond).check())
#else
#define assert(cond) ((void)0)
#endif

#endif // BETTER_ASSERT_HPP
