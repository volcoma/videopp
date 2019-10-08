#pragma once

#include "draw_cmd.h"
#include "text.h"
#include "polyline.h"

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

const program_setup& empty_setup() noexcept;

//-----------------------------------------------------------------------------
/// Calculates the scale transform needed for fitting an item into an area.
//-----------------------------------------------------------------------------
math::transformf fit_item(float item_w, float item_h,
                          float area_w, float area_h,
                          size_fit sz_fit,
                          dimension_fit dim_fit);
/// A draw list. Contains draw commands. Can be reused.
struct draw_list
{
    draw_list();

    //-----------------------------------------------------------------------------
    /// Clears the list.
    //-----------------------------------------------------------------------------
    void clear() noexcept;

    //-----------------------------------------------------------------------------
    /// Pushes/pops a clip rect to be used in the following commands
    //-----------------------------------------------------------------------------
    void push_clip(const rect& clip);
    void pop_clip();

    //-----------------------------------------------------------------------------
    /// Pushes/pops a blending to be used in the following commands, otherwise
    /// it will automatically try to detect it based on the texture format.
    //-----------------------------------------------------------------------------
    void push_blend(blending_mode blend);
    void pop_blend();

    //-----------------------------------------------------------------------------
    /// Adds a rect to the list.
    //-----------------------------------------------------------------------------
    void add_rect(const rect& r,
                  const color& col = color::white(),
                  bool filled = true,
                  float thickness = 1.0f);

    void add_rect(const rect& r,
                  const math::transformf& transform,
                  const color& col = color::white(),
                  bool filled = true,
                  float thickness = 1.0f);

    void add_rect(const frect& r,
                  const math::transformf& transform,
                  const color& col = color::white(),
                  bool filled = true,
                  float thickness = 1.0f);

    void add_rect(const std::array<math::vec2, 4>& points,
                  const color& col = color::white(),
                  bool filled = true,
                  float thickness = 1.0f);

    //-----------------------------------------------------------------------------
    /// Adds a line to the list.
    //-----------------------------------------------------------------------------
    void add_line(const math::vec2& start,
                  const math::vec2& end,
                  const color& col = color::white(),
                  float thickness = 1.0f);


    //-----------------------------------------------------------------------------
    /// Adds an image to the list.
    //-----------------------------------------------------------------------------
    void add_image(texture_view texture,
                   const rect& src,
                   const rect& dst,
                   const math::transformf& transform,
                   const color& col = color::white(),
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(texture_view texture,
                   const rect& src,
                   const rect& dst,
                   const color& col = color::white(),
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(texture_view texture,
                   const rect& dst,
                   const color& col = color::white(),
                   math::vec2 min_uv = {0.0f, 0.0f},
                   math::vec2 max_uv = {1.0f, 1.0f},
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(texture_view texture,
                   const rect& dst,
                   const math::transformf& transform,
                   const color& col = color::white(),
                   math::vec2 min_uv = {0.0f, 0.0f},
                   math::vec2 max_uv = {1.0f, 1.0f},
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(texture_view texture,
                   const point& pos,
                   const color& col = color::white(),
                   math::vec2 min_uv = {0.0f, 0.0f},
                   math::vec2 max_uv = {1.0f, 1.0f},
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(texture_view texture,
                   const std::array<math::vec2, 4>& points,
                   const color& col = color::white(),
                   math::vec2 min_uv = {0.0f, 0.0f},
                   math::vec2 max_uv = {1.0f, 1.0f},
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_vertices(draw_type dr_type,
                      const vertex_2d* vertices,
                      size_t count,
                      primitive_type type,
                      texture_view texture = {},
                      const program_setup& setup = empty_setup());
    //-----------------------------------------------------------------------------
    /// Adds another draw_list into the list
    //-----------------------------------------------------------------------------
    void add_list(const draw_list& list);

    //-----------------------------------------------------------------------------
    /// Adds a text which will be fitted into the destination rect.
    /// Position inside the rect is affected by the text's alignment and transform.
    //-----------------------------------------------------------------------------
    void add_text(const text& t,
                  const math::transformf& transform,
                  const rect& dst_rect,
                  size_fit sz_fit = size_fit::shrink_to_fit,
                  dimension_fit dim_fit = dimension_fit::uniform);

    //-----------------------------------------------------------------------------
    /// Adds a text to the list.
    /// The script part lies on the ascent of the whole part.
    //-----------------------------------------------------------------------------
    void add_text(const text& t,
                  const math::transformf& transform);


    //-----------------------------------------------------------------------------
    /// Adds a supercript text to the list.
    /// The script part lies on the ascent of the whole part.
    //-----------------------------------------------------------------------------
    void add_text_superscript(const text& whole_text,
                              const text& script_text,
                              const math::transformf& transform,
                              float script_scale = 0.7f);

    //-----------------------------------------------------------------------------
    /// Adds a supercript text which will be fitted into the destination rect.
    /// Position inside the rect is affected by the text's alignment and transform.
    //-----------------------------------------------------------------------------
    void add_text_superscript(const text& whole_text,
                              const text& script_text,
                              const math::transformf& transform,
                              const rect& dst_rect,
                              float script_scale = 0.7f,
                              size_fit sz_fit = size_fit::shrink_to_fit,
                              dimension_fit dim_fit = dimension_fit::uniform);

    //-----------------------------------------------------------------------------
    /// Adds a subscript text to the list.
    /// The script part lies on the baseline of the whole part.
    //-----------------------------------------------------------------------------
    void add_text_subscript(const text& whole_text,
                            const text& script_text,
                            const math::transformf& transform,
                            float script_scale = 0.7f);

    //-----------------------------------------------------------------------------
    /// Adds a subscript text which will be fitted into the destination rect.
    /// The script part lies on the baseline of the whole part.
    /// Position inside the rect is affected by the text's alignment and transform.
    //-----------------------------------------------------------------------------
    void add_text_subscript(const text& whole_text,
                            const text& script_text,
                            const math::transformf& transform,
                            const rect& dst_rect,
                            float script_scale = 0.7f,
                            size_fit sz_fit = size_fit::shrink_to_fit,
                            dimension_fit dim_fit = dimension_fit::uniform);

    //-----------------------------------------------------------------------------
    /// Adds a polyline to the list.
    //-----------------------------------------------------------------------------
    void add_polyline(const polyline& poly,
                      const color& col,
                      bool closed,
                      float thickness = 1.0f,
                      float antialias_size = 1.0f);

    void add_polyline_gradient(const polyline& poly,
                               const color& coltop,
                               const color& colbot,
                               bool closed,
                               float thickness = 1.0f,
                               float antialias_size = 1.0f);

    //-----------------------------------------------------------------------------
    /// Adds a polyline as a filled convex to the list.
    /// Will only work for convex shapes
    //-----------------------------------------------------------------------------
    void add_polyline_filled_convex(const polyline& poly,
                                    const color& colf,
                                    float antialias_size = 1.0f);

    void add_polyline_filled_convex_gradient(const polyline& poly,
                                             const color& coltop,
                                             const color& colbot,
                                             float antialias_size = 1.0f);

    //-----------------------------------------------------------------------------
    /// Adds an ellipse to the list.
    //-----------------------------------------------------------------------------
    void add_ellipse(const math::vec2& center,
                     const math::vec2& radii,
                     const color& col,
                     size_t num_segments = 12,
                     float thickness = 1.0f);

    void add_ellipse_gradient(const math::vec2& center,
                              const math::vec2& radii,
                              const color& col1,
                              const color& col2,
                              size_t num_segments = 12,
                              float thickness = 1.0f);

    //-----------------------------------------------------------------------------
    /// Adds a filled ellipse to the list.
    //-----------------------------------------------------------------------------
    void add_ellipse_filled(const math::vec2& center,
                            const math::vec2& radii,
                            const color& col,
                            size_t num_segments = 12);

    void add_bezier_curve(const math::vec2& pos0,
                          const math::vec2& cp0,
                          const math::vec2& cp1,
                          const math::vec2& pos1,
                          const color& col,
                          float thickness = 1.0f,
                          int num_segments = 0);


    void add_curved_path_gradient(const std::vector<math::vec2>& points,
                                  const color& c1,
                                  const color& c2,
                                  float thickness = 1.0f,
                                  float antialias_size = 1.0f);

    //-----------------------------------------------------------------------------
    /// Reserve vertices
    //-----------------------------------------------------------------------------
    void reserve_vertices(size_t count);
    void reserve_rects(size_t count);


    //-----------------------------------------------------------------------------
    ///
    //-----------------------------------------------------------------------------
    void add_text_superscript_impl(const text& whole_text,
                                   const text& script_text,
                                   const math::transformf& transform,
                                   float script_scale);
    void add_text_subscript_impl(const text& whole_text,
                                 const text& script_text,
                                 const math::transformf& transform,
                                 float script_scale);

    //-----------------------------------------------------------------------------
    /// Debugging facilities
    //-----------------------------------------------------------------------------
    void add_text_debug_info(const text& t, const math::transformf& transform);
    std::string to_string() const;
    void validate_stacks() const noexcept;
    static void set_debug_draw(bool debug);
    static void toggle_debug_draw();

    //-----------------------------------------------------------------------------
    /// Data members
    //-----------------------------------------------------------------------------
    using index_t = uint32_t;
    /// vertices to draw
    std::vector<vertex_2d> vertices;
    /// indices to draw
    std::vector<index_t> indices;
    /// draw commands
    std::vector<draw_cmd> commands;
    /// clip rects stack
    std::vector<rect> clip_rects;
    /// clip rects stack
    std::vector<blending_mode> blend_modes;
    /// total commands requested
    size_t commands_requested = 0;
};

}
