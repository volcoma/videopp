#pragma once

#include "math/transform.hpp"

namespace gfx
{
    template<typename T>
    struct size_type
    {
        T w {};
        T h {};

        constexpr size_type() noexcept = default;
        constexpr size_type(T w, T h) noexcept : w(w), h(h) {}
    };

    using size = size_type<int>;
    using sizef = size_type<float>;

    //Arithmetic operations on size
    template<typename T>
    inline size_type<T> operator+(const size_type<T>& lhs, const size_type<T>& rhs)
    {
        return size_type<T>(lhs.w + rhs.w, lhs.h + rhs.h);
    }

    template<typename T>
    inline size_type<T> operator-(const size_type<T>& lhs, const size_type<T>& rhs)
    {
        return size_type<T>(lhs.w - rhs.w, lhs.h - rhs.h);
    }

    template<typename T, typename S>
    inline typename std::enable_if<std::is_arithmetic<S>::value, size_type<T>>::type
    operator+(const size_type<T>& lhs, const S& rhs)
    {
        return size_type<T>(lhs.w + rhs, lhs.h + rhs);
    }

    template<typename T, typename S>
    inline typename std::enable_if<std::is_arithmetic<S>::value, size_type<T>>::type
    operator-(const size_type<T>& lhs, const S& rhs)
    {
        return size_type<T>(lhs.w - rhs, lhs.h - rhs);
    }

    template<typename T, typename S>
    inline typename std::enable_if<std::is_arithmetic<S>::value, size_type<T>>::type
    operator*(const size_type<T>& lhs, const S& rhs)
    {
        return size_type<T>(lhs.w * rhs, lhs.h * rhs);
    }
}
