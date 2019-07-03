
#include "draw_list.h"
#include <cmath>
#include <codecvt>
#include <locale>

#include "logger.h"
#include "font.h"
#include "renderer.h"
#include "text.h"

namespace video_ctrl
{
namespace detail
{

float align_to_pixel(float val)
{
    return val;//static_cast<float>(static_cast<int>(val));
}

math::vec2 align_to_pixel(math::vec2 val)
{
    return val;//{align_to_pixel(val.x), align_to_pixel(val.y)};
}

std::array<math::vec2, 4> transform_rect(const rect& r, const math::transformf& transform)
{
    math::vec2 lt = {r.x, r.y};
    math::vec2 rt = {r.x + r.w, r.y};
    math::vec2 rb = {r.x + r.w, r.y + r.h};
    math::vec2 lb = {r.x, r.y + r.h};

    auto p0 = align_to_pixel(transform.transform_coord(lt));
    auto p1 = align_to_pixel(transform.transform_coord(rt));
    auto p2 = align_to_pixel(transform.transform_coord(rb));
    auto p3 = align_to_pixel(transform.transform_coord(lb));

    std::array<math::vec2, 4> points = {{p0, p1, p2, p3}};
    return points;
}


bool can_be_batched(const draw_cmd& cmd, uint64_t hash, primitive_type type)
{
    return cmd.hash == hash && type != primitive_type::lines_loop;
}

void add_indices(draw_list& list, std::uint32_t vertices_added, std::uint32_t index_offset,
                 primitive_type type, const program_setup& setup = {})
{
    const auto indices_before = std::uint32_t(list.indices.size());
    switch(type)
    {
        case primitive_type::triangles:
        {
            // indices added will be (vertices_added - 2) * 3;
            const uint32_t rect_vertices = 4;
            const uint32_t rects = vertices_added / rect_vertices;
            for(uint32_t r = 0; r < rects; ++r)
            {
                for(std::uint32_t i = 2; i < rect_vertices; ++i)
                {
                    list.indices.emplace_back(index_offset + 0);
                    list.indices.emplace_back(index_offset + i - 1);
                    list.indices.emplace_back(index_offset + i);
                }
                index_offset += rect_vertices;
            }
        }
        break;
        case primitive_type::lines:
        case primitive_type::lines_loop:
        {
            if(vertices_added >= 2)
            {
                for(std::uint32_t i = 0; i < vertices_added - 1; ++i)
                {
                    list.indices.emplace_back(index_offset + i);
                    list.indices.emplace_back(index_offset + i + 1);
                }
            }
        }
        break;
        default:
        break;
    }

    auto add_new_command = [&](uint64_t hash)
    {
        list.commands.emplace_back();
        auto& command = list.commands.back();
        command.type = type;
        command.indices_offset = indices_before;
        command.setup = setup;
        command.hash = hash;
    };

    if(!setup.calc_uniforms_hash)
    {
        add_new_command(0);
    }
    else
    {
        auto hash = setup.calc_uniforms_hash();
        utils::hash(hash, type, setup.program.shader.get());

        if(list.commands.empty() || !can_be_batched(list.commands.back(), hash, type))
        {
            add_new_command(hash);
        }
    }


    const std::uint32_t indices_added = std::uint32_t(list.indices.size()) - indices_before;
    auto& command = list.commands.back();
    command.indices_offset = std::min(command.indices_offset, indices_before);
    command.indices_count += indices_added;
}
}

const program_setup& get_default_color_setup()
{
    static auto program = []()
    {
        program_setup program;
        program.program = simple_program();

        return program;
    }();

    return program;
}

uint32_t add_rect_primitive(std::vector<vertex_2d>& vertices, const std::array<math::vec2, 4>& points,
                            const color& col, const math::vec2& min_uv, const math::vec2& max_uv)
{
    vertices.emplace_back();
    auto& v1 = vertices.back();
    v1.pos = points[0];
    v1.uv = min_uv;
    v1.col = col;

    vertices.emplace_back();
    auto& v2 = vertices.back();
    v2.pos = points[1];
    v2.uv = {max_uv.x, min_uv.y};
    v2.col = col;

    vertices.emplace_back();
    auto& v3 = vertices.back();
    v3.pos = points[2];
    v3.uv = max_uv;
    v3.col = col;

    vertices.emplace_back();
    auto& v4 = vertices.back();
    v4.pos = points[3];
    v4.uv = {min_uv.x, max_uv.y};
    v4.col = col;

    return 4;
}

void draw_list::add_rect(const std::array<math::vec2, 4>& points, const color& col, bool filled,
                         float border_width, const program_setup& setup)
{
    const math::vec2 min_uv = {0.0f, 0.0f};
    const math::vec2 max_uv = {1.0f, 1.0f};
    const auto index_offset = std::uint32_t(vertices.size());

    const auto type = filled ? primitive_type::triangles : primitive_type::lines_loop;

    const auto vertices_added = add_rect_primitive(vertices, points, col, min_uv, max_uv);

    auto program = setup;

    if(program.program.shader == nullptr)
    {
        program.program = simple_program();
        program.calc_uniforms_hash = [=]()
        {
            uint64_t seed{0};
            utils::hash(seed, border_width);
            return seed;
        };
        program.begin = [=](const gpu_context& ctx)
        {
            ctx.rend.set_line_width(border_width);
        };
    }

    detail::add_indices(*this, vertices_added, index_offset, type, program);
}

void draw_list::add_rect(const std::vector<vertex_2d>& verts, bool filled, float line_width,
                         const program_setup& setup)
{
    const auto type = filled ? primitive_type::triangles : primitive_type::lines_loop;
    add_vertices(verts, type, line_width, {}, setup);
}

draw_list::draw_list()
{
    reserve_rects(32);
}

void draw_list::add_rect(const rect& dst, const color& col, bool filled, float border_width,
                         const program_setup& setup)
{
    const std::array<math::vec2, 4> points = {{math::vec2(dst.x, dst.y), math::vec2(dst.x + dst.w, dst.y),
                                               math::vec2(dst.x + dst.w, dst.y + dst.h),
                                               math::vec2(dst.x, dst.y + dst.h)}};

    add_rect(points, col, filled, border_width, setup);
}

void draw_list::add_rect(const rect& r, const math::transformf& transform, const color& col, bool filled,
                         float border_width, const program_setup& setup)
{
    // If we want this to be batched with other calls we need to
    // do the transformations on the cpu side
    const auto points = detail::transform_rect(r, transform);
    add_rect(points, col, filled, border_width, setup);
}

void draw_list::add_line(const math::vec2& start, const math::vec2& end, const color& col, float line_width,
                         const program_setup& setup)
{

    const math::vec2 min_uv = {0.0f, 0.0f};
    const math::vec2 max_uv = {1.0f, 1.0f};
    const auto index_offset = std::uint32_t(vertices.size());

    vertices.emplace_back();
    auto& v1 = vertices.back();
    v1.pos = start;
    v1.uv = min_uv;
    v1.col = col;

    vertices.emplace_back();
    auto& v2 = vertices.back();
    v2.pos = end;
    v2.uv = {max_uv.x, min_uv.y};
    v2.col = col;

    const auto type = primitive_type::lines;

    auto program = setup;

    if(program.program.shader == nullptr)
    {
        program.program = simple_program();
        program.calc_uniforms_hash = [=]()
        {
            uint64_t seed{0};
            utils::hash(seed, line_width);
            return seed;
        };
        program.begin = [=](const gpu_context& ctx)
        {
            ctx.rend.set_line_width(line_width);
        };
    }

    detail::add_indices(*this, 2, index_offset, type, program);
}

void draw_list::add_image(const texture_view& texture, const rect& src, const rect& dst,
                          const math::transformf& transform, const color& col, const program_setup& setup)
{
    const auto rect = texture ? video_ctrl::rect{0, 0, int(texture.width), int(texture.height)} : src;
    float left = static_cast<float>(src.x) / rect.w;
    float right = static_cast<float>(src.x + src.w) / rect.w;
    float top = static_cast<float>(src.y) / rect.h;
    float bottom = static_cast<float>(src.y + src.h) / rect.h;
    const math::vec2 min_uv = {left, top};
    const math::vec2 max_uv = {right, bottom};

    const auto points = detail::transform_rect(dst, transform);
    add_image(texture, points, col, min_uv, max_uv, setup);
}

void draw_list::add_image(const texture_view& texture, const rect& src, const rect& dst, const color& col,
                          const program_setup& setup)
{
    const auto rect = texture ? video_ctrl::rect{0, 0, int(texture.width), int(texture.height)} : src;
    float left = static_cast<float>(src.x) / rect.w;
    float right = static_cast<float>(src.x + src.w) / rect.w;
    float top = static_cast<float>(src.y) / rect.h;
    float bottom = static_cast<float>(src.y + src.h) / rect.h;
    const math::vec2 min_uv = {left, top};
    const math::vec2 max_uv = {right, bottom};

    add_image(texture, dst, col, min_uv, max_uv, setup);
}

void draw_list::add_image(const texture_view& texture, const rect& dst, const color& col,
                          const math::vec2& min_uv, const math::vec2& max_uv, const program_setup& setup)
{
    std::array<math::vec2, 4> points = {{math::vec2(dst.x, dst.y), math::vec2(dst.x + dst.w, dst.y),
                                         math::vec2(dst.x + dst.w, dst.y + dst.h),
                                         math::vec2(dst.x, dst.y + dst.h)}};

    add_image(texture, points, col, min_uv, max_uv, setup);
}

void draw_list::add_image(const texture_view& texture, const rect& dst, const math::transformf& transform,
                          const color& col, const math::vec2& min_uv, const math::vec2& max_uv,
                          const program_setup& setup)
{
    const auto points = detail::transform_rect(dst, transform);
    add_image(texture, points, col, min_uv, max_uv, setup);
}

void draw_list::add_image(const texture_view& texture, const point& pos, const color& col,
                          const math::vec2& min_uv, const math::vec2& max_uv, const program_setup& setup)
{
    const auto rect = texture ? video_ctrl::rect{pos.x, pos.y, int(texture.width), int(texture.height)}
                              : video_ctrl::rect{pos.x, pos.y, 0, 0};
    add_image(texture, rect, col, min_uv, max_uv, setup);
}

void draw_list::add_image(const texture_view& texture, const std::array<math::vec2, 4>& points,
                          const color& col, const math::vec2& min_uv, const math::vec2& max_uv, const program_setup& setup)
{
    const auto index_offset = std::uint32_t(vertices.size());

    const auto vertices_added = add_rect_primitive(vertices, points, col, min_uv, max_uv);

    auto program = setup;

    if(program.program.shader == nullptr)
    {
        if(texture)
        {
            program.program = multi_channel_texture_program();
            program.calc_uniforms_hash = [=]()
            {
                uint64_t seed{0};
                utils::hash(seed, texture);
                return seed;
            };
            program.begin = [texture](const gpu_context& ctx)
            {
                ctx.program.shader->set_uniform("uTexture", texture);
            };
        }
        else
        {
            program.program = simple_program();
        }

    }

    detail::add_indices(*this, vertices_added, index_offset, primitive_type::triangles, program);
}

void draw_list::add_text(const text& t, const math::transformf& transform, const rect& dst, size_fit sz_fit,
                         dimension_fit dim_fit, const program_setup& setup)
{

    // TODO this doesn't handle if any rotation
    // is applied
    const auto& scale = transform.get_scale();
    float text_width = t.get_width() * scale.x;
    float text_height = t.get_height() * scale.y;

    math::transformf scale_trans;
    float xscale = 1.0f;
    float yscale = 1.0f;

    switch(sz_fit)
    {
        case size_fit::shrink_to_fit:
        {
            if(text_width > dst.w)
            {
                xscale = std::min(xscale, float(dst.w) / text_width);
            }
            if(text_height > dst.h)
            {
                yscale = std::min(yscale, float(dst.h) / text_height);
            }
        }
        break;

        case size_fit::stretch_to_fit:
        {
            if(text_width < dst.w)
            {
                xscale = std::max(xscale, float(dst.w) / text_width);
            }
            if(text_height < dst.h)
            {
                yscale = std::max(yscale, float(dst.h) / text_height);
            }
        }
        break;

        case size_fit::auto_fit:
        {
            if(text_width > dst.w)
            {
                xscale = std::min(xscale, float(dst.w) / text_width);
            }
            else
            {
                xscale = std::max(xscale, float(dst.w) / text_width);
            }

            if(text_height > dst.h)
            {
                yscale = std::min(yscale, float(dst.h) / text_height);
            }
            else
            {
                yscale = std::max(yscale, float(dst.h) / text_height);
            }
        }
    }

    switch(dim_fit)
    {
        case dimension_fit::x:
            scale_trans.set_scale(xscale, 1.0f, 1.0f);
            break;

        case dimension_fit::y:
            scale_trans.set_scale(1.0f, yscale, 1.0f);
            break;

        case dimension_fit::uniform:
            float uniform_scale = std::min(xscale, yscale);
            scale_trans.set_scale(uniform_scale, uniform_scale, 1.0f);
            break;
    }

    add_text(t, transform * scale_trans, setup);
}

void draw_list::add_text(const text& t, const math::transformf& transform, const program_setup& setup)
{
    if(!t.is_valid())
    {
        return;
    }

    const auto& offsets = t.get_shadow_offsets();
    if(math::any(math::notEqual(offsets, math::vec2(0.0f, 0.0f))))
    {
        auto shadow = t;
        shadow.set_color(t.get_shadow_color());
        shadow.set_outline_color(t.get_shadow_color());
        shadow.set_shadow_offsets({0.0f, 0.0f});
        math::transformf shadow_transform;
        shadow_transform.translate(offsets.x, offsets.y, 0.0f);
        add_text(shadow, transform * shadow_transform, setup);
    }

    const auto& font = t.get_font();
    float unit_length_in_pixels_at_font_position = 1.0f;
    float sdf_spread = font->sdf_spread;
    float pixel_density = font->pixel_density;
    float scale = std::max(transform.get_scale().x, transform.get_scale().y);
    float distance_field_multiplier = (2 * sdf_spread + 1) * pixel_density * unit_length_in_pixels_at_font_position * scale;
    float outline_width = std::max(0.0f, t.get_outline_width());
    color outline_color = t.get_outline_color();

    // cpu_batching is disabled for text rendering with more than X vertices
    // because there are too many vertices and their matrix multiplication
    // on the cpu dominates the batching benefits.
    const auto& geometry = t.get_geometry();
    bool cpu_batch = geometry.size() < (16 * 4);

    auto program = setup;
    auto texture = font->texture;
    if(setup.program.shader == nullptr)
    {
        if(sdf_spread > 0)
        {
            program.program = distance_field_font_program();

            if(cpu_batch)
            {
                program.calc_uniforms_hash = [=]()
                {
                    uint64_t seed{0};
                    utils::hash(seed,
                                distance_field_multiplier,
                                outline_width,
                                outline_color,
                                texture);
                    return seed;
                };
            }
            program.begin = [=](const gpu_context& ctx)
            {
                if(!cpu_batch)
                {
                    ctx.rend.push_transform(transform);
                }
                ctx.program.shader->set_uniform("uDistanceFieldMultiplier", distance_field_multiplier);
                ctx.program.shader->set_uniform("uOutlineWidth", outline_width);
                ctx.program.shader->set_uniform("uOutlineColor", outline_color);
                ctx.program.shader->set_uniform("uTexture", texture, 0, texture::wrap_type::wrap_clamp);
            };
        }
        else
        {
            switch(texture->get_pix_type())
            {
                case pix_type::gray:
                    program.program = single_channel_texture_program();
                break;

                default:
                    program.program = multi_channel_texture_program();
                break;
            }

            if(cpu_batch)
            {
                program.calc_uniforms_hash = [=]()
                {
                    uint64_t seed{0};
                    utils::hash(seed, texture);
                    return seed;
                };
            }
            program.begin = [=](const gpu_context& ctx)
            {
                if(!cpu_batch)
                {
                    ctx.rend.push_transform(transform);
                }
                ctx.program.shader->set_uniform("uTexture", texture, 0, texture::wrap_type::wrap_clamp);
            };

        }

    }

    if(cpu_batch)
    {
        auto transformed_geometry = geometry;
        for(auto& v : transformed_geometry)
        {
            v.pos = transform.transform_coord(v.pos);
            v.pos.x = detail::align_to_pixel(v.pos.x);
            v.pos.y = detail::align_to_pixel(v.pos.y);
        }
        add_vertices(transformed_geometry, primitive_type::triangles, 0.0f, texture, program);
    }
    else
    {
        program.end = [](const gpu_context& ctx)
        {
            ctx.rend.pop_transform();
        };

        add_vertices(geometry, primitive_type::triangles, 0.0f, texture, program);
    }
}

void draw_list::add_text_superscript(const font_ptr& font, const std::string& whole,
                                     const std::string& partial, const math::transformf& transform,
                                     const text::alignment align, const color& col,
                                     float outline_width, const color& outline_col,
                                     float partial_scale, const program_setup& setup)
{
    video_ctrl::text text_whole;
    text_whole.set_font(font);
    text_whole.set_utf8_text(whole);
    text_whole.set_alignment(align);
    text_whole.set_color(col);
    text_whole.set_outline_color(outline_col);
    text_whole.set_outline_width(outline_width);

    video_ctrl::text text_partial;
    text_partial.set_font(font);
    text_partial.set_utf8_text(partial);
    text_partial.set_alignment(align);
    text_partial.set_color(col);
    text_partial.set_outline_color(outline_col);
    text_partial.set_outline_width(outline_width);

    add_text_superscript(text_whole, text_partial, transform, align, partial_scale, setup);
}

void draw_list::add_text_superscript(const text& text_whole,
                                     const text& text_partial,
                                     const math::transformf& transform,
                                     text::alignment align,
                                     float partial_scale,
                                     const program_setup& setup)
{
    math::transformf transform_whole;
    math::transformf transform_partial;
    transform_partial.scale({partial_scale, partial_scale, 1.0f});

    add_text_superscript_impl(text_whole, text_partial, transform, transform_whole, transform_partial, align, partial_scale, setup);
}

void draw_list::add_text_superscript(const text& whole_text,
                                     const text& partial_text,
                                     const math::transformf& transform,
                                     const rect& dst_rect,
                                     text::alignment align,
                                     float partial_scale,
                                     size_fit sz_fit,
                                     dimension_fit dim_fit,
                                     const program_setup& setup)
{
    math::transformf transform_whole;
    math::transformf transform_partial;

    transform_partial.scale({partial_scale, partial_scale, 1.0f});


    auto scale_whole = transform_whole.get_scale() * transform.get_scale();
    auto scale_partial = transform_partial.get_scale() * transform.get_scale();
    float text_width = whole_text.get_width() * scale_whole.x +
                       partial_text.get_width() * scale_partial.x;
    float text_height = whole_text.get_height() * scale_whole.y;

    math::transformf scale_trans;
    float xscale = 1.0f;
    float yscale = 1.0f;

    switch(sz_fit)
    {
        case size_fit::shrink_to_fit:
        {
            if(text_width > dst_rect.w)
            {
                xscale = std::min(xscale, float(dst_rect.w) / text_width);
            }
            if(text_height > dst_rect.h)
            {
                yscale = std::min(yscale, float(dst_rect.h) / text_height);
            }
        }
        break;

        case size_fit::stretch_to_fit:
        {
            if(text_width < dst_rect.w)
            {
                xscale = std::max(xscale, float(dst_rect.w) / text_width);
            }
            if(text_height < dst_rect.h)
            {
                yscale = std::max(yscale, float(dst_rect.h) / text_height);
            }
        }
        break;

        case size_fit::auto_fit:
        {
            if(text_width > dst_rect.w)
            {
                xscale = std::min(xscale, float(dst_rect.w) / text_width);
            }
            else
            {
                xscale = std::max(xscale, float(dst_rect.w) / text_width);
            }

            if(text_height > dst_rect.h)
            {
                yscale = std::min(yscale, float(dst_rect.h) / text_height);
            }
            else
            {
                yscale = std::max(yscale, float(dst_rect.h) / text_height);
            }
        }
    }

    switch(dim_fit)
    {
        case dimension_fit::x:
            scale_trans.set_scale(xscale, 1.0f, 1.0f);
            break;

        case dimension_fit::y:
            scale_trans.set_scale(1.0f, yscale, 1.0f);
            break;

        case dimension_fit::uniform:
            float uniform_scale = std::min(xscale, yscale);
            scale_trans.set_scale(uniform_scale, uniform_scale, 1.0f);
            break;
    }


    add_text_superscript_impl(whole_text, partial_text, transform * scale_trans, transform_whole, transform_partial, align, partial_scale, setup);
}

void draw_list::add_text_superscript_impl(const text& text_whole, const text& text_partial, const math::transformf& transform, math::transformf& transform_whole, math::transformf& transform_partial, text::alignment align, float partial_scale, const program_setup& setup)
{
    if(text_whole.get_lines().size() > 1)
    {
        log("Superscript text should not be multiline. This api will not behave properly.");
    }
    const auto text_whole_width = detail::align_to_pixel(text_whole.get_width());

    const auto text_whole_min_baseline_height = detail::align_to_pixel(text_whole.get_min_baseline_height());
    const auto text_whole_max_baseline_height = detail::align_to_pixel(text_whole.get_max_baseline_height());

    const auto text_partial_width = detail::align_to_pixel(text_partial.get_width() * partial_scale);
    const auto text_parital_height = detail::align_to_pixel(text_partial.get_height() * (1.0f - partial_scale));

    const auto text_partial_min_baseline_height = detail::align_to_pixel(text_partial.get_min_baseline_height() * (partial_scale));
    const auto text_partial_max_baseline_height = detail::align_to_pixel(text_partial.get_max_baseline_height() * (partial_scale));


    const auto half_text_whole_width = detail::align_to_pixel(text_whole_width * 0.5f);
    const auto half_text_partial_width = detail::align_to_pixel(text_partial_width * 0.5f);
    const auto half_text_parital_height = detail::align_to_pixel(text_parital_height * 0.5f);

    switch(align)
    {
        case text::alignment::top_left:
        {
            transform_partial.translate({text_whole_width, 0.0f, 0.0f});
        }
        break;
        case text::alignment::baseline_top_left:
        {
            transform_partial.translate({text_whole_width, 0.0f, 0.0f});
            transform_partial.translate({0.0f, -(text_whole_min_baseline_height - text_partial_min_baseline_height), 0.0f});
        }
        break;

        case text::alignment::top:
        {
            transform_whole.translate({-half_text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({half_text_whole_width, 0.0f, 0.0f});
        }
        break;
        case text::alignment::baseline_top:
        {
            transform_whole.translate({-half_text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({half_text_whole_width, -(text_whole_min_baseline_height - text_partial_min_baseline_height), 0.0f});
        }
        break;

        case text::alignment::top_right:
        {
            transform_whole.translate({-text_partial_width, 0.0f, 0.0f});
        }
        break;
        case text::alignment::baseline_top_right:
        {
            transform_whole.translate({-text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({0.0f, -(text_whole_min_baseline_height - text_partial_min_baseline_height), 0.0f});

        }
        break;

        case text::alignment::left:
        {
            transform_partial.translate({text_whole_width, -half_text_parital_height, 0.0f});
        }
        break;

        case text::alignment::center:
        {
            transform_whole.translate({-half_text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({half_text_whole_width, -half_text_parital_height, 0.0f});
        }
        break;

        case text::alignment::right:
        {
            transform_whole.translate({-text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({0.0f, -half_text_parital_height, 0.0f});
        }
        break;

        case text::alignment::bottom_left:
        {
            transform_partial.translate({text_whole_width, -text_parital_height, 0.0f});
        }
        break;
        case text::alignment::baseline_bottom_left:
        {
            transform_partial.translate({text_whole_width, -(text_whole_max_baseline_height - text_partial_max_baseline_height), 0.0f});
        }
        break;

        case text::alignment::bottom:
        {
            transform_whole.translate({-half_text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({half_text_whole_width, -text_parital_height, 0.0f});
        }
        break;
        case text::alignment::baseline_bottom:
        {
            transform_whole.translate({-half_text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({half_text_whole_width, -(text_whole_max_baseline_height - text_partial_max_baseline_height), 0.0f});
        }
        break;

        case text::alignment::bottom_right:
        {
            transform_whole.translate({-text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({0.0f, -text_parital_height, 0.0f});
        }
        break;
        case text::alignment::baseline_bottom_right:
        {
            transform_whole.translate({-text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({0.0f, -(text_whole_max_baseline_height - text_partial_max_baseline_height), 0.0f});
        }
        break;
        default:
        break;
    }
    add_text(text_whole, transform * transform_whole, setup);
    add_text(text_partial, transform * transform_partial, setup);
}


void draw_list::add_vertices(const std::vector<vertex_2d>& verts, const primitive_type type, float line_width,
                             const texture_view& texture, const program_setup& setup)
{
    add_vertices(verts.data(), verts.size(), type, line_width, texture, setup);
}

void draw_list::add_vertices(const vertex_2d* verts, size_t count, primitive_type type, float line_width, const texture_view &texture, const program_setup &setup)
{
    if(count == 0)
    {
        return;
    }
    const auto index_offset = std::uint32_t(vertices.size());
    vertices.resize(index_offset + count);
    std::memcpy(&vertices[index_offset], verts, sizeof(vertex_2d) * count);

    auto program = setup;
    if(program.program.shader == nullptr)
    {
        if(texture)
        {
            program.program = multi_channel_texture_program();
            program.calc_uniforms_hash = [=]()
            {
                uint64_t seed{0};
                utils::hash(seed, texture);
                return seed;
            };
            program.begin = [texture](const gpu_context& ctx)
            {
                ctx.program.shader->set_uniform("uTexture", texture);
            };
        }
        else
        {
            program.program = simple_program();
            program.calc_uniforms_hash = [=]()
            {
                uint64_t seed{0};
                utils::hash(seed, line_width);
                return seed;
            };
            program.begin = [=](const gpu_context& ctx)
            {
                ctx.rend.set_line_width(line_width);
            };
        }
    }

    detail::add_indices(*this, std::uint32_t(count), index_offset, type, program);
}


void draw_list::reserve_vertices(size_t count)
{
    vertices.reserve(vertices.size() + count);
    // for a typical triangle list
    // indices reserced will be (count - 2) * 3;
    const size_t indices_reserved = (count - 2) * 3;
    indices.reserve(indices.size() + indices_reserved);
}

void draw_list::reserve_rects(size_t count)
{
    reserve_vertices(count * 4);
}

void draw_list::add_text_debug_info(const text& t, const math::transformf& transform)
{
    math::transformf tr;
    tr.set_scale(transform.get_scale());

    {
        auto geometry = t.get_geometry();
        for(auto& v : geometry)
        {
            v.pos = transform.transform_coord(v.pos);
            v.pos.x = detail::align_to_pixel(v.pos.x);
            v.pos.y = detail::align_to_pixel(v.pos.y);
        }
        for(size_t i = 0; i < geometry.size(); i+=4)
        {
            add_vertices(&geometry[i], 4, primitive_type::lines_loop);
        }

    }

    const auto& rect = t.get_rect();
    {
        auto col = color::cyan();
        std::string desc = "ascent ";
        const auto& lines = t.get_ascent_lines();
        for(const auto& line : lines)
        {
            auto v1 = transform.transform_coord({rect.x, line});
            auto v2 = transform.transform_coord({rect.x + rect.w, line});

            add_line(v1, v2, col);

            text txt;
            txt.set_color(col);
            txt.set_font(default_font());
            txt.set_alignment(text::alignment::right);
            txt.set_utf8_text(desc);

            tr.set_position(v1.x, v1.y, 0.0f);
            add_text(txt, tr);

//            txt.set_alignment(text::alignment::left);
//            txt.set_utf8_text(" width = " + std::to_string(t.get_width()));

//            tr.set_position(v2.x, v1.y, 0.0f);
//            add_text(txt, tr);
        }
    }
    {
        auto col = color::magenta();
        std::string desc = "baseline ";
        const auto& lines = t.get_baseline_lines();
        for(const auto& line : lines)
        {
            auto v1 = transform.transform_coord({rect.x, line});
            auto v2 = transform.transform_coord({rect.x + rect.w, line});

            add_line(v1, v2, col, 2.0f);

            text txt;
            txt.set_color(col);
            txt.set_font(default_font());
            txt.set_alignment(text::alignment::right);
            txt.set_utf8_text(desc);

            tr.set_position(v1.x, v1.y, 0.0f);
            add_text(txt, tr);
        }
    }
    {
        auto col = color::blue();
        std::string desc = "descent ";
        const auto& lines = t.get_descent_lines();
        for(const auto& line : lines)
        {
            auto v1 = transform.transform_coord({rect.x, line});
            auto v2 = transform.transform_coord({rect.x + rect.w, line});

            add_line(v1, v2, col, 2.0f);

            text txt;
            txt.set_color(col);
            txt.set_font(default_font());
            txt.set_alignment(text::alignment::right);
            txt.set_utf8_text(desc);

            tr.set_position(v1.x, v1.y, 0.0f);
            add_text(txt, tr);
        }
    }

    {
        auto col = color::green();
        std::string desc = " line height = ";
        const auto& ascent_lines = t.get_ascent_lines();
        for(auto line : ascent_lines)
        {
            auto line_height = t.get_font()->line_height;
            auto coord = rect.x + rect.w;
            auto v1 = transform.transform_coord({coord, line});
            auto v2 = transform.transform_coord({coord, line + line_height});

            add_line(v1, v2, col, 2.0f);

//            text txt;
//            txt.set_color(col);
//            txt.set_font(default_font());
//            txt.set_alignment(text::alignment::left);
//            txt.set_utf8_text(desc + std::to_string(line_height));

//            tr.set_position(v1.x + 4.0f, v1.y + (v2.y - v1.y) * 0.5f, 0.0f);
//            add_text(txt, tr);

        }
    }

}

}
