#pragma once

#include "point.h"
#include "size.h"
#include "utils.h"

namespace gfx
{
    template<typename T>
    struct rect_t
    {
        T x {};
        T y {};
        T w {};
        T h {};

        constexpr rect_t() noexcept = default;
        constexpr rect_t(T x, T y, T w, T h) noexcept : x(x), y(y), w(w), h(h) {}
        constexpr rect_t(T x, T y, const size_type<T>& size) noexcept : x(x), y(y), w(size.w), h(size.h) {}
        constexpr rect_t(const point_t<T>& pos, T w, T h) noexcept : x(pos.x), y(pos.y), w(w), h(h) {}
        constexpr rect_t(const point_t<T>& pos, const size_type<T>& size) noexcept : x(pos.x), y(pos.y), w(size.w), h(size.h) {}

        rect_t(const rect_t &other) noexcept  = default;
        rect_t(rect_t &&other) noexcept = default;
        
        // getter
        inline point_t<T> get_pos() const noexcept { return {x, y}; }
        inline size_type<T> get_size() const noexcept { return {w, h}; }

        // setter
        void set_pos(const point_t<T>& pos) noexcept { x = pos.x; y = pos.y; }
        void set_size(const size_type<T>& size) noexcept { w = size.w; h = size.h; }

        rect_t offset(const point_t<T>& pt) const
        {
            rect_t copy = *this;
            copy.x += pt.x;
            copy.y += pt.y;
            return copy;
        }

        /// Copy operator (Dimo: Why not use default one? The comparison inside is bad.)
        rect_t& operator = (const rect_t &other) noexcept
        {
            if (this != &other)
            {
                x = other.x;
                y = other.y;
                w = other.w;
                h = other.h;
            }
            return *this;
        }

        /// Check if this rectangle is inside another
        ///     @param other - the rectangle to check if this is inside of it
        ///     @return true if this rect is inside other
        bool is_inner_of(const rect_t &other) const noexcept
        {
            if (x >= other.x && y >= other.y && (x + w <= other.x + other.w) && (y + h <= other.y + other.h))
                return true;
    
            return false;
        }
        
        /// Check if rectangles overlap at some point
        ///     @param other - the rectangle to check for overlap
        ///     @return true if this rect overlaps the other
        bool is_overlapping(const rect_t &other) const noexcept
        {
            auto left_x = std::max(x, other.x);
            auto right_x = std::min(x + w, other.x + other.w);
            auto top_y = std::max(y, other.y);
            auto bottom_y = std::min(y + h, other.y + other.h);

            if (left_x < right_x && top_y < bottom_y)
            {
                return true;
            }

            return false;
        }
        
        /// Get the area rectangle of the overlap (a.k.a. crop)
        ///     @param other - the rectangle to check for overlap
        ///     @return the are rectangle of the overlap
        rect_t get_overlapping_rect(const rect_t &other) const noexcept
        {
            auto left_x = std::max(x, other.x);
            auto right_x = std::min(x + w, other.x + other.w);
            auto top_y = std::max(y, other.y);
            auto bottom_y = std::min(y + h, other.y + other.h);

            if (left_x < right_x && top_y < bottom_y)
            {
                return {left_x, top_y, right_x - left_x, bottom_y - top_y};
            }

            return {};
        }
        
        /// Get normalized texture coordinates after cropping
        ///     @param cropper - part of texture to use
        ///     @return the texture coords (left, right, top, bottom limits, represented as a rect)
        rect_t get_cropped_texture_coord(const rect_t &cropper) const
        {
            rect_t result;
            result.x = static_cast<float> (cropper.x) / w;
            result.y = static_cast<float> (cropper.x + cropper.w) / w;
            result.w = static_cast<float> (cropper.y) / h;
            result.h = static_cast<float> (cropper.y + cropper.h) / h;
            return result;
        }
        
        /// Check if rectangle contains a point
        ///     @param p - point to check
        ///     @return true if p is inside this rect
        bool contains(const point_t<T>& p) const
        {
            return (p.x >= x && p.x <= (x + w) && p.y >= y && p.y <= (y + h));            
        }


        void insert(const point_t<T>& pt)
        {
            if(pt.x < x)
            {
                x = pt.x;
            }
            else if(pt.x > x + w)
            {
                w = pt.x - x;
            }

            if(pt.y < y)
            {
                y = pt.y;
            }
            else if( pt.y > y + h)
            {
                h = pt.y - y;
            }
        }

        bool operator==(const rect_t &obj) const noexcept
        {
            // Dimo: This is bad, we should use ULPs for float comparisons
            return x == obj.x && y == obj.y && w == obj.w && h == obj.h;            
        }
        
        bool operator!=(const rect_t &obj) const noexcept
        {
            return !(*this == obj);            
        }

        /// Less compare
        bool operator<(const rect_t &obj) const noexcept
        {
            return x < obj.x || y < obj.y || w < obj.w || h < obj.h;            
        }

        /// Cast to true if rectangle has a valid area (>0)
        explicit operator bool() const noexcept
        {
            return w != 0 && h != 0;
        }

        void expand(const T& x, const T& y) noexcept
        {
            this->x -= x;
            this->y -= y;
            this->w += 2 * x;
            this->h += 2 * y;
        }
    };
    
    using rect = rect_t<int>;
    using frect = rect_t<float>;
    using tcoords = frect;
}

namespace std
{
    template<> struct hash<gfx::rect>
    {
        using argument_type = gfx::rect;
        using result_type = std::size_t;
        result_type operator()(argument_type const& s) const noexcept
        {
            uint64_t seed{0};
            utils::hash(seed, s.x, s.y, s.w, s.h);
            return seed;
        }
    };
}

