#pragma once

#include "rect.h"
#include "color.h"

#include <cstdint>
#include <vector>


namespace video_ctrl
{


namespace corner_flags
{
    enum : uint32_t
    {
        top_left   = 1 << 0, // 0x1
        top_right  = 1 << 1, // 0x2
        bot_left   = 1 << 2, // 0x4
        bot_right  = 1 << 3, // 0x8
        top       = top_left | top_right,   // 0x3
        bot       = bot_left | bot_right,   // 0xC
        left      = top_left | bot_left,    // 0x5
        right     = top_right | bot_right,  // 0xA
        all       = 0xF     // In your function calls you may use ~0 (= all bits sets) instead of ImDrawCornerFlags_All, as a convenience
    };
}


class polyline
{
public:
    void clear();
    void line_to(const math::vec2& pos);
    void arc_to(const math::vec2& centre, float radius, float a_min, float a_max, size_t num_segments = 10);
    void arc_to_negative(const math::vec2& centre, float radius, float a_min, float a_max, size_t num_segments = 10);

    void arc_to_fast(const math::vec2& centre, float radius, size_t a_min_of_12,
                       size_t a_max_of_12); // Use precomputed angles for a 12 steps circle
    void bezier_curve_to(const math::vec2& p1, const math::vec2& p2, const math::vec2& p3,
                           int num_segments = 0);
    void rectangle(const math::vec2& rect_min, const math::vec2& rect_max, float rounding = 0.0f,
                  uint32_t rounding_corners_flags = corner_flags::all);

    void rectangle(const rect& r, float rounding = 0.0f,
                  uint32_t rounding_corners_flags = corner_flags::all);

    void set_color(const color& col) noexcept{ color_ = col; }
    const color& get_color() const noexcept { return color_; }

    void set_thickness(float thickness) noexcept{ thickness_ = thickness; }
    float get_thickness() const noexcept { return thickness_; }

    void set_closed(bool closed) noexcept { closed_ = closed; }
    bool get_closed() const { return closed_; }

    void set_antialiased(bool antialiased) noexcept { antialiased_ = antialiased; }
    bool get_antialiased() const noexcept { return antialiased_; }

    const std::vector<math::vec2>& get_points() const { return points_; }
private:
    std::vector<math::vec2> points_;
    color color_;
    float thickness_{1.0f};
    bool closed_{};
    bool antialiased_{true};
};
}
