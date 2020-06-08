#pragma once
#include "math/transform.hpp"
#include "utils.h"

namespace gfx
{
    template<typename T>
    struct point_t
    {
        T x {}, y {};

        constexpr point_t() noexcept = default;
        constexpr point_t(T x, T y) noexcept : x(x), y(y) {}
    };
    using point = point_t<int>;
    using pointf = point_t<float>;
    
    template<typename T>
    inline bool operator==(const point_t<T>& lhs, const point_t<T>& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y;
    }

    template<typename T>
    inline bool operator!=(const point_t<T>& lhs, const point_t<T>& rhs)
    {
        return !operator==(lhs, rhs);
    }

    //Arithmetic operations on point
    template<typename T>
    inline point_t<T> operator+(const point_t<T>& lhs, const point_t<T>& rhs)
    {
        return point_t<T>(lhs.x + rhs.x, lhs.y + rhs.y);
    }
    
    template<typename T>
    inline point_t<T> operator-(const point_t<T>& lhs, const point_t<T>& rhs)
    {
        return point_t<T>(lhs.x - rhs.x, lhs.y - rhs.y);
    }
    
    template<typename T, typename S>
    inline typename std::enable_if<std::is_arithmetic<S>::value, point_t<T>>::type
    operator+(const point_t<T>& lhs, const S& rhs)
    {
        return point_t<T>(lhs.x + rhs, lhs.y + rhs);
    }
    
    template<typename T, typename S>
    inline typename std::enable_if<std::is_arithmetic<S>::value, point_t<T>>::type
    operator-(const point_t<T>& lhs, const S& rhs)
    {
        return point_t<T>(lhs.x - rhs, lhs.y - rhs);
    }
    
    template<typename T, typename S>
    inline typename std::enable_if<std::is_arithmetic<S>::value, point_t<T>>::type
    operator*(const point_t<T>& lhs, const S& rhs)
    {
        return point_t<T>(lhs.x * rhs, lhs.y * rhs);
    }
}

namespace std
{

template<>
struct hash<gfx::point>
{
    size_t operator()(const gfx::point& p) const noexcept
    {
        uint64_t seed = 0;
        utils::hash(seed, p.x, p.y);
        return seed;
    }
};

}
