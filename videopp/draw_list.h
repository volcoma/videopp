#pragma once

#include "draw_cmd.h"
#include "text.h"
#include "polyline.h"
#include "rich_text.h"

namespace gfx
{

struct draw_config
{
    /// maximum number of different textures
    /// that can be batched in one draw call.
    size_t max_textures_per_batch{32};

    /// cpu_batching is disabled for text rendering with more than X vertices
    /// because there are too many vertices and their matrix multiplication
    /// on the cpu dominates the batching benefits.
    size_t max_cpu_transformed_glyhps{24};

    /// when linear filtering, we need to shift uv coords to the
    /// center of the texels, otherwise for outermost pixels
    /// the filtering will sample the image's neighbouring pixels in the atlas.
    math::vec2 filtering_correction{0.5f, 0.0f};

    /// Supersampling when signed distance field(vectorized)
    /// texts are rendered. Results in better visuals when
    /// text is scaled down, at the cost of some gpu performance.
    bool sdf_supersample{true};

    /// Internally uses mapped buffers for vertices/indices update.
    bool mapped_buffers{false};

    /// Enable/Disable debug draw.
    bool debug{};
};

const draw_config& get_draw_config();
void set_draw_config(const draw_config& cfg);
bool debug_draw();

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
    uniform,
	non_uniform
};


enum class movie_format
{
    opaque,
    valpha,
    halpha
};
const program_setup& empty_setup() noexcept;

//-----------------------------------------------------------------------------
/// Calculates the scale transform needed for fitting an item into an area.
//-----------------------------------------------------------------------------
math::transformf fit_item(float item_w, float item_h,
                          float area_w, float area_h,
                          size_fit sz_fit = size_fit::shrink_to_fit,
                          dimension_fit dim_fit = dimension_fit::uniform);

math::transformf align_item(align_t align,
                            float minx, float miny,
                            float maxx, float maxy,
                            bool pixel_snap);

math::transformf align_item(align_t align,
                            const frect& item);

math::transformf align_and_fit_item(align_t align,
                                    float item_w, float item_h,
                                    const math::transformf& transform,
                                    const frect& dst_rect,
                                    size_fit sz_fit = size_fit::shrink_to_fit,
                                    dimension_fit dim_fit = dimension_fit::uniform);

math::transformf align_and_fit_text(const text& t,
                                    const math::transformf& transform,
                                    const frect& dst_rect,
                                    size_fit sz_fit = size_fit::shrink_to_fit,
                                    dimension_fit dim_fit = dimension_fit::uniform);

math::transformf align_wrap_and_fit_text(text& t,
                                         const math::transformf& transform,
                                         frect dst_rect,
                                         size_fit sz_fit = size_fit::shrink_to_fit,
                                         dimension_fit dim_fit = dimension_fit::uniform);

using source_data = std::tuple<texture_view, rect>;

/// A draw list. Contains draw commands. Can be reused.
struct draw_list
{
    using index_t = uint32_t;
    using crop_area_t = std::vector<rect>;

    draw_list(bool has_debug_info = true);
    draw_list(draw_list&&) = default;
    draw_list& operator=(draw_list&&) = default;
    ~draw_list();

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
    /// Pushes/pops crop rects to be used in the following commands
    //-----------------------------------------------------------------------------
    void push_crop(const crop_area_t& crop);
    void pop_crop();

    //-----------------------------------------------------------------------------
    /// Pushes/pops a blending to be used in the following commands, otherwise
    /// it will automatically try to detect it based on the texture format.
    //-----------------------------------------------------------------------------
    void push_blend(blending_mode blend);
    void pop_blend();

    //-----------------------------------------------------------------------------
    /// Pushes/pops a global transform to be used in the following commands.
    //-----------------------------------------------------------------------------
    void push_transform(const math::transformf& tr);
    void pop_transform();

    //-----------------------------------------------------------------------------
    /// Pushes/pops a program to be used in the following commands if they do not
    /// specify one.
    //-----------------------------------------------------------------------------
    void push_program(const gpu_program& program);
    void pop_program();
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
    /// Adds a movie image to the list.
    //-----------------------------------------------------------------------------
    void add_movie_image(const texture_view& texture,
                         const rect& src,
                         const rect& dst,
                         const math::transformf& transform,
                         movie_format format = movie_format::opaque,
                         flip_format flip = flip_format::none,
                         const color& col = color::white());

    void add_movie_image(const texture_view& texture,
                         const rect& src,
                         const rect& dst,
                         movie_format format = movie_format::opaque,
                         flip_format flip = flip_format::none,
                         const color& col = color::white());

    void add_movie_image(const texture_view& texture,
                         const rect& dst,
                         movie_format format = movie_format::opaque,
                         flip_format flip = flip_format::none,
                         const color& col = color::white());

    void add_movie_image(const texture_view& texture,
                         const rect& dst,
                         const math::transformf& transform,
                         movie_format format = movie_format::opaque,
                         flip_format flip = flip_format::none,
                         const color& col = color::white());

    void add_movie_image(const texture_view& texture,
                         const math::transformf& transform,
                         movie_format format = movie_format::opaque,
                         flip_format flip = flip_format::none,
                         const color& col = color::white());

    //-----------------------------------------------------------------------------
    /// Adds a movie images (rgb and alpha textures) to the list.
    //-----------------------------------------------------------------------------
    void add_movie_images(const texture_view& rgb_texture,
                          const texture_view& alpha_texture,
                          const rect& src,
                          const rect& dst,
                          const math::transformf& transform,
                          const color& col = color::white(),
                          flip_format flip = flip_format::none);

    void add_movie_images(const texture_view& rgb_texture,
                          const texture_view& alpha_texture,
                          const rect& src,
                          const rect& dst,
                          const color& col = color::white(),
                          flip_format flip = flip_format::none);

    void add_movie_images(const texture_view& rgb_texture,
                          const texture_view& alpha_texture,
                          const rect& dst,
                          const color& col = color::white(),
                          flip_format flip = flip_format::none);

    void add_movie_images(const texture_view& rgb_texture,
                          const texture_view& alpha_texture,
                          const rect& dst,
                          const math::transformf& transform,
                          const color& col = color::white(),
                          flip_format flip = flip_format::none);

    void add_movie_images(const texture_view& rgb_texture,
                          const texture_view& alpha_texture,
                          const math::transformf& transform,
                          const color& col = color::white(),
                          flip_format flip = flip_format::none);

    //-----------------------------------------------------------------------------
    /// Adds an image to the list.
    //-----------------------------------------------------------------------------
    void add_image(const texture_view& texture,
                   const rect& src,
                   const rect& dst,
                   const math::transformf& transform,
                   const color& col = color::white(),
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(const texture_view& texture,
                   const rect& src,
                   const rect& dst,
                   const color& col = color::white(),
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(const texture_view& texture,
                   const rect& dst,
                   const color& col = color::white(),
                   math::vec2 min_uv = {0.0f, 0.0f},
                   math::vec2 max_uv = {1.0f, 1.0f},
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(const texture_view& texture,
                   const rect& dst,
                   const math::transformf& transform,
                   const color& col = color::white(),
                   math::vec2 min_uv = {0.0f, 0.0f},
                   math::vec2 max_uv = {1.0f, 1.0f},
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(const source_data& src,
                   const rect& dst,
                   const color& col = color::white(),
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(const source_data& src,
                   const math::transformf& transform,
                   const rect& dst,
                   const color& col = color::white(),
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(const source_data& src,
                   const math::transformf& transform,
                   const color& col = color::white(),
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(const source_data& src,
                   const point& dst,
                   const color& col = color::white(),
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(const texture_view& texture,
                   const point& pos,
                   const color& col = color::white(),
                   math::vec2 min_uv = {0.0f, 0.0f},
                   math::vec2 max_uv = {1.0f, 1.0f},
                   flip_format flip = flip_format::none,
                   const program_setup& setup = empty_setup());

    void add_image(const texture_view& texture,
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
                      const texture_view& texture = {},
                      const program_setup& setup = empty_setup());
    //-----------------------------------------------------------------------------
    /// Adds another draw_list into the list
    //-----------------------------------------------------------------------------
    void add_list(const draw_list& list, bool transform_verts = true);

    //-----------------------------------------------------------------------------
    /// Adds a text which will be fitted into the destination rect.
    /// Position inside the rect is affected by the text's alignment and transform.
    ///
    /// Example below with different aligmnents and zero position, and dst_rect.
    //-----------------------------------------------------------------------------
    //    +-------------------+   +-------------------+   +-------------------+
    //    |top_left           |   |        top        |   |          top_right|
    //    |                   |   |                   |   |                   |
    //    |                   |   |                   |   |                   |
    //    +-------------------+   +-------------------+   +-------------------+
    ///
    //    +-------------------+   +-------------------+   +-------------------+
    //    |                   |   |                   |   |                   |
    //    |left               |   |       center      |   |              right|
    //    |                   |   |                   |   |                   |
    //    +-------------------+   +-------------------+   +-------------------+
    ///
    //    +-------------------+   +-------------------+   +-------------------+
    //    |                   |   |                   |   |                   |
    //    |                   |   |                   |   |                   |
    //    |bottom_left        |   |       bottom      |   |       bottom_right|
    //    +-------------------+   +-------------------+   +-------------------+
    //-----------------------------------------------------------------------------
    void add_text(const text& t,
                  const math::transformf& transform,
                  const frect& dst_rect,
                  size_fit sz_fit = size_fit::shrink_to_fit,
                  dimension_fit dim_fit = dimension_fit::uniform);
    void add_text(const rich_text& t,
                  const math::transformf& transform,
                  const frect& dst_rect,
                  size_fit sz_fit = size_fit::shrink_to_fit,
                  dimension_fit dim_fit = dimension_fit::uniform);

    //-----------------------------------------------------------------------------
    /// Adds a text to the list.
    /// The script part lies on the ascent of the whole part.
    //-----------------------------------------------------------------------------
    void add_text(const text& t,
                  const math::transformf& transform);
    void add_text(const rich_text& t,
                  const math::transformf& transform);

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
    /// Debugging facilities
    //-----------------------------------------------------------------------------
    void add_text_debug_info(const text& t, const math::transformf& transform);
    std::string to_string() const;
    void validate_stacks() const noexcept;
    void apply_linear_filtering_correction(const texture_view& texture,
                                           math::vec2& min_uv,
                                           math::vec2& max_uv);

    bool empty() const noexcept { return commands_requested == 0; }
    //-----------------------------------------------------------------------------
    /// Data members
    //-----------------------------------------------------------------------------
    /// vertices to draw
    std::vector<vertex_2d> vertices;
    /// indices to draw
    std::vector<index_t> indices;
    /// draw commands
    std::vector<draw_cmd> commands;
    /// clip rects stack
    std::vector<rect> clip_rects;
    /// crop rects stack
    std::vector<crop_area_t> crop_areas;
    /// blend modes stack
    std::vector<blending_mode> blend_modes;
    /// transforms stack
    std::vector<math::transformf> transforms;
    /// programs stack
    std::vector<gpu_program> programs;
    /// total commands requested
    int commands_requested = 0;

    std::unique_ptr<draw_list> debug;
};

std::string to_string(size_fit fit);
std::string to_string(dimension_fit fit);

template<> size_fit from_string<size_fit>(const std::string& str);
template<> dimension_fit from_string<dimension_fit>(const std::string& str);

}
