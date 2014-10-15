#ifndef ARRAY_VIEW_HPP
#define ARRAY_VIEW_HPP

#include "better_assert.hpp"

#include <cstddef>

template <typename T>
class ArrayView {
    T* b = nullptr;
    T* e = nullptr;

public:

    using value_type = T;

    ArrayView() = default;
    ArrayView(T* b, T* e) : b(b), e(e) {};
    ArrayView(T* b, size_t sz) : b(b), e(b+sz) {};

    T* begin() {
        ArrayView const& self = *this;
        return const_cast<T*>(self.begin());
    }

    T* end() {
        ArrayView const& self = *this;
        return const_cast<T*>(self.end());
    }

    T const* begin() const {
        return b;
    }

    T const* end() const {
        return e;
    }

    auto size() const {
        return (e-b);
    }

    bool empty() const {
        return (b == e);
    }

    T& operator[](std::size_t i) {
        ArrayView const& self = *this;
        return const_cast<T&>(self.operator[](i));
    }

    T const& operator[](std::size_t i) const {
        assert(i >= 0 && i <= size());
        return b[i];
    }

    ArrayView slice(size_t from, size_t to) const {
        assert(!empty());
        assert(from >= 0 && from <= to && to <= size());
        return ArrayView(b+from,b+to);
    }

    ArrayView slice(size_t from) const {
        assert(b);
        assert(from >= 0 && from <= size());
        return ArrayView(b+from,e);
    }

    bool operator==(ArrayView const& other) const {
        return (b==other.b && e==other.e);
    }
};

#endif
