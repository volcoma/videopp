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

    void arc_to(const math::vec2& centre, const math::vec2& radii, float a_min, float a_max, size_t num_segments = 10);
    void arc_to_negative(const math::vec2& centre, const math::vec2& radii, float a_min, float a_max, size_t num_segments = 10);

    void arc_to_fast(const math::vec2& centre, float raius, size_t a_min_of_12,
                       size_t a_max_of_12); // Use precomputed angles for a 12 steps circle
    void arc_to_fast(const math::vec2& centre, const math::vec2& radii, size_t a_min_of_12,
                       size_t a_max_of_12); // Use precomputed angles for a 12 steps circle

    void bezier_curve_to(const math::vec2& p1, const math::vec2& p2, const math::vec2& p3,
                           int num_segments = 0);
    void rectangle(const math::vec2& rect_min, const math::vec2& rect_max, float rounding = 0.0f,
                  uint32_t rounding_corners_flags = corner_flags::all);

    void rectangle(const rect& r, float rounding = 0.0f,
                  uint32_t rounding_corners_flags = corner_flags::all);

    const std::vector<math::vec2>& get_points() const { return points_; }
private:
    std::vector<math::vec2> points_;
};
}
