#ifndef RANGES_HPP
#define RANGES_HPP

#include <iterator>
#include <utility>

//Iterator reversor
template <typename Iter>
std::reverse_iterator<Iter> reverse(Iter i) {
    return std::reverse_iterator<Iter>(i);
}

// Allows use of range-based for loop on raw iterators

template <typename T, typename U>
struct IteratorRange {
    T b {};
    U e {};

    IteratorRange() = default;
    IteratorRange(T bi, U ei) : b(std::move(bi)), e(std::move(ei)) {}

    T begin() const {
        return b;
    }

    U end() const {
        return e;
    }
};

template <typename I1, typename I2>
auto iter_range(I1 i1, I2 i2) {
    return IteratorRange<I1,I2>{std::move(i1),std::move(i2)};
}

//Allows numeric sequences in range-for

template <typename T, typename U>
struct NumberRange {
    T b {};
    U e {};

    template <typename Num>
    struct NumberIterator {
        Num n {};

        NumberIterator(Num n) : n(std::move(n)) {}

        NumberIterator& operator++() {
            ++n;
            return *this;
        }

        Num const& operator*() const {
            return n;
        }

        template <typename Num2>
        bool operator!=(NumberIterator<Num2> const& b) const {
            return (n != b.n);
        }
    };

    NumberRange() = default;
    NumberRange(T bi, U ei) : b(std::move(bi)), e(std::move(ei)) {}

    NumberIterator<T> begin() const {
        return NumberIterator<T>{b};
    }

    NumberIterator<U> end() const {
        return NumberIterator<U>{e};
    }
};

template <typename I1, typename I2>
auto number_range(I1 i1, I2 i2) {
    return NumberRange<I1,I2>{std::move(i1),std::move(i2)};
}

//Allows use of range-based for loops wit acces to iterator.

template <typename T, typename U>
struct DeferredRange {
    template <typename Iter>
    struct DeferredIterator {
        Iter iter {};

        DeferredIterator() = default;
        DeferredIterator(Iter iter) : iter(std::move(iter)) {}

        DeferredIterator& operator++() {
            ++iter;
            return *this;
        }

        Iter const& operator*() const {
            return iter;
        }

        template <typename Iter2>
        bool operator!=(DeferredIterator<Iter2> const& b) const {
            return (iter != b.iter);
        }
    };

    DeferredIterator<T> b {};
    DeferredIterator<U> e {};

    DeferredRange() = default;
    DeferredRange(T bi, U ei) : b(std::move(bi)), e(std::move(ei)) {}

    DeferredIterator<T> const& begin() const {
        return b;
    }

    DeferredIterator<U> const& end() const {
        return e;
    }
};

template <typename Container>
auto defer_range(Container& c) {
    return DeferredRange<decltype(begin(c)),decltype(end(c))>{begin(c),end(c)};
}

//Reverse range-based for loop
template <typename T, typename U>
struct ReversedRange {
    T b {};
    U e {};

    ReversedRange() = default;
    ReversedRange(T bi, U ei) : b(std::move(bi)), e(std::move(ei)) {}

    T begin() const {
        return b;
    }

    U end() const {
        return e;
    }
};

template <typename Container>
auto reverse_range(Container& c) {
    return ReversedRange<decltype(c.rbegin()),decltype(c.rend())>{c.rbegin(),c.rend()};
}

#endif // RANGES_HPP
