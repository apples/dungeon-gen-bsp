#ifndef ARRAY_VIEW_HPP
#define ARRAY_VIEW_HPP

#include <cstddef>

template <typename T>
class ArrayView {
    T* b = nullptr;
    T* e = nullptr;

public:

    using value_type = T;

    ArrayView() = default;
    ArrayView(T* b, T* e) : b(b), e(e) {};

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
        return b[i];
    }
};

#endif
