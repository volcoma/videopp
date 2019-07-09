#pragma once

#include "draw_cmd.h"
#include "text.h"

namespace video_ctrl
{

enum class size_fit
{
    shrink_to_fit,
    stretch_to_fit,
    auto_fit
};

enum class dimension_fit
{
    x,
    y,
    uniform
};

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

struct polyline
{
    void line_to(const math::vec2& pos);
    void line_to_merge_duplicate(const math::vec2& pos);
    void arc_to(const math::vec2& centre, float radius, float a_min, float a_max, size_t num_segments = 10);
    void arc_to_fast(const math::vec2& centre, float radius, size_t a_min_of_12,
                       size_t a_max_of_12); // Use precomputed angles for a 12 steps circle
    void bezier_curve_to(const math::vec2& p1, const math::vec2& p2, const math::vec2& p3,
                           int num_segments = 0);
    void rectangle(const math::vec2& rect_min, const math::vec2& rect_max, float rounding = 0.0f,
                  uint32_t rounding_corners_flags = corner_flags::all);

    std::vector<math::vec2> points;
    color col;
    float thickness{1.0f};
    bool closed{};
    bool antialiased{true};
};

/// A draw list. Contains draw commands. Can be reused.
struct draw_list
{
    draw_list();

    void add_rect(const rect& r, const color& col = color::white(), bool filled = true,
                  float border_width = 0.0f, const program_setup& setup = {});

    void add_rect(const rect& r, const math::transformf& transform, const color& col = color::white(),
                  bool filled = true, float border_width = 0.0f, const program_setup& setup = {});

    void add_rect(const std::array<math::vec2, 4>& points, const color& col = color::white(),
                  bool filled = true, float line_width = 0.0f, const program_setup& setup = {});

    void add_rect(const std::vector<vertex_2d>& verts, bool filled = true, float line_width = 0.0f,
                  const program_setup& setup = {});

    void add_line(const math::vec2& start, const math::vec2& end, const color& col = color::white(),
                  float line_width = 1.0f, const program_setup& setup = {});

    void add_image(const texture_view& texture, const rect& src, const rect& dst,
                   const math::transformf& transform, const color& col = color::white(),
                   const program_setup& setup = {});

    void add_image(const texture_view& texture, const rect& src, const rect& dst,
                   const color& col = color::white(), const program_setup& setup = {});

    void add_image(const texture_view& texture, const rect& dst, const color& col = color::white(),
                   const math::vec2& min_uv = {0.0f, 0.0f}, const math::vec2& max_uv = {1.0f, 1.0f},
                   const program_setup& setup = {});

    void add_image(const texture_view& texture, const rect& dst, const math::transformf& transform,
                   const color& col = color::white(), const math::vec2& min_uv = {0.0f, 0.0f},
                   const math::vec2& max_uv = {1.0f, 1.0f}, const program_setup& setup = {});

    void add_image(const texture_view& texture, const point& pos, const color& col = color::white(),
                   const math::vec2& min_uv = {0.0f, 0.0f}, const math::vec2& max_uv = {1.0f, 1.0f},
                   const program_setup& setup = {});

    void add_image(const texture_view& texture, const std::array<math::vec2, 4>& points,
                   const color& col = color::white(), const math::vec2& min_uv = {0.0f, 0.0f},
                   const math::vec2& max_uv = {1.0f, 1.0f}, const program_setup& setup = {});

    void add_text(const text& t, const math::transformf& transform, const rect& dst_rect,
                  size_fit sz_fit = size_fit::shrink_to_fit, dimension_fit dim_fit = dimension_fit::uniform,
                  const program_setup& setup = {});

    void add_text(const text& t, const math::transformf& transform, const program_setup& setup = {});


    void add_text_superscript(const text& whole_text,
                              const text& partial_text,
                              const math::transformf& transform,
                              text::alignment align = text::alignment::top_left,
                              float partial_scale = 0.7f,
                              const program_setup& setup = {});

    void add_text_superscript(const text& whole_text,
                              const text& partial_text,
                              const math::transformf& transform,
                              const rect& dst_rect,
                              text::alignment align = text::alignment::top_left,
                              float partial_scale = 0.7f,
                              size_fit sz_fit = size_fit::shrink_to_fit,
                              dimension_fit dim_fit = dimension_fit::uniform,
                              const program_setup& setup = {});

    void add_text_subscript(const text& whole_text,
                              const text& partial_text,
                              const math::transformf& transform,
                              text::alignment align = text::alignment::top_left,
                              float partial_scale = 0.7f,
                              const program_setup& setup = {});

    void add_text_subscript(const text& whole_text,
                            const text& partial_text,
                            const math::transformf& transform,
                            const rect& dst_rect,
                            text::alignment align = text::alignment::top_left,
                            float partial_scale = 0.7f,
                            size_fit sz_fit = size_fit::shrink_to_fit,
                            dimension_fit dim_fit = dimension_fit::uniform,
                            const program_setup& setup = {});

    void add_vertices(const std::vector<vertex_2d>& verts, primitive_type type, float line_width = 1.0f,
                      const texture_view& texture = {}, const program_setup& setup = {});
    void add_vertices(const vertex_2d* verts, size_t count, primitive_type type, float line_width = 1.0f,
                      const texture_view& texture = {}, const program_setup& setup = {});

    void add_polyline(const polyline& poly);
    void add_polyline_filled(const polyline& poly);

    void add_circle(const math::vec2& center, float radius, const color& col, size_t num_segments = 12, float thickness = 1.0f);
    void add_circle_filled(const math::vec2& center, float radius, const color& col, size_t num_segments = 12);
    void add_bezier_curve(const math::vec2& pos0, const math::vec2& cp0, const math::vec2& cp1, const math::vec2& pos1, const color& col, float thickness = 1.0f, int num_segments = 0);

    void push_clip(const rect& clip);
    void pop_clip();

    void reserve_vertices(size_t count);
    void reserve_rects(size_t count);

    void add_text_debug_info(const text& t, const math::transformf& transform);
    void add_text_superscript_impl(const text& whole_text,
                                   const text& partial_text,
                                   const math::transformf& transform,
                                   math::transformf& whole_transform,
                                   math::transformf& partial_transform,
                                   text::alignment align,
                                   float partial_scale,
                                   const program_setup& setup);

    void add_text_subscript_impl(const text& whole_text,
                                 const text& partial_text,
                                 const math::transformf& transform,
                                 math::transformf& whole_transform,
                                 math::transformf& partial_transform,
                                 text::alignment align,
                                 float partial_scale,
                                 const program_setup& setup);

    using index_t = uint32_t;
    std::vector<vertex_2d> vertices; // vertices to draw
    std::vector<index_t> indices;    // indices to draw
    std::vector<draw_cmd> commands;  // draw commands

    std::vector<rect> clip_rects;
    int commands_requested = 0;
};
}
