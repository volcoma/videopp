
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

std::array<math::vec2, 4> transform_rect(const rect& r, const math::transformf& transform)
{
    math::vec2 lt = {r.x, r.y};
    math::vec2 rt = {r.x + r.w, r.y};
    math::vec2 rb = {r.x + r.w, r.y + r.h};
    math::vec2 lb = {r.x, r.y + r.h};

    auto p0 = transform.transform_coord(lt);
    auto p1 = transform.transform_coord(rt);
    auto p2 = transform.transform_coord(rb);
    auto p3 = transform.transform_coord(lb);

    std::array<math::vec2, 4> points = {{p0, p1, p2, p3}};
    return points;
}

uint64_t simple_hash() noexcept
{
    uint64_t seed{0};
    utils::hash(seed, 0.0f);
    return seed;
}

uint64_t calc_final_hash(uint64_t hash, primitive_type type, const program_setup& setup)
{
    utils::hash(hash, type, setup.program.shader);
    return hash;
}

inline bool can_be_batched(const draw_cmd& cmd, uint64_t hash) noexcept
{
    return cmd.hash == hash;
}

inline bool can_be_batched(draw_list& list, uint64_t hash) noexcept
{
    return !list.commands.empty() && detail::can_be_batched(list.commands.back(), hash);
}


template<typename Setup>
void add_indices(draw_list& list, std::uint32_t vertices_added, std::uint32_t index_offset,
                 primitive_type type, Setup&& setup)
{
    const auto indices_before = std::uint32_t(list.indices.size());
    std::uint32_t indices_added = 0;
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
                    indices_added += 3;
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
                    indices_added += 2;
                }
            }
        }
        break;
        default:
            break;
    }

    rect clip{};
    if(!list.clip_rects.empty())
    {
        clip = list.clip_rects.back();
    }
    const auto add_new_command = [&](uint64_t hash) {
        list.commands.emplace_back();
        auto& command = list.commands.back();
        command.type = type;
        command.indices_offset = indices_before;
        command.setup = std::forward<Setup>(setup);
        command.hash = hash;
        command.clip_rect = clip;
    };

    list.commands_requested++;
    if(setup.uniforms_hash == 0)
    {
        add_new_command(0);
    }
    else
    {
        auto hash = detail::calc_final_hash(setup.uniforms_hash, type, setup);
        if(!can_be_batched(list, hash))
        {
            add_new_command(hash);
        }
    }

    auto& command = list.commands.back();
    command.indices_offset = std::min(command.indices_offset, indices_before);
    command.indices_count += indices_added;
}


void prim_reserve(draw_list& list, size_t idx_count, size_t vtx_count, size_t& idx_current_idx, size_t& vtx_current_idx)
{
    auto& cmd = list.commands.back();
    cmd.indices_count += idx_count;

    idx_current_idx = list.indices.size();
    vtx_current_idx = list.vertices.size();
    list.indices.resize(list.indices.size() + idx_count);
    list.vertices.resize(list.vertices.size() + vtx_count);
}
}

const program_setup& get_simple_setup() noexcept
{
    static auto setup = []() {
        program_setup setup;
        setup.program = simple_program();
        setup.uniforms_hash = detail::simple_hash();
        return setup;
    }();

    return setup;
}

const program_setup& empty_setup() noexcept
{
    static program_setup setup;
    return setup;
}

uint32_t add_rect_primitive(std::vector<vertex_2d>& vertices, const std::array<math::vec2, 4>& points,
                            const color& col, const math::vec2& min_uv, const math::vec2& max_uv)
{
    vertices.emplace_back(points[0], min_uv, col);
    vertices.emplace_back(points[1], math::vec2{max_uv.x, min_uv.y}, col);
    vertices.emplace_back(points[2], max_uv, col);
    vertices.emplace_back(points[3], math::vec2{min_uv.x, max_uv.y}, col);

    return 4;
}

draw_list::draw_list()
{
    reserve_rects(32);
}

void draw_list::add_rect(const std::array<math::vec2, 4>& points, const color& col, bool filled,
                         float thickness)
{
    polyline line;
    for(const auto& p : points)
    {
        line.line_to(p);
    }

    if(filled)
    {
        add_polyline_filled_convex(line, col, 1.0f);
    }
    else
    {
        add_polyline(line, col, true, thickness, 1.0f);
    }
}

void draw_list::add_rect(const rect& dst, const color& col, bool filled, float thickness)
{
    const std::array<math::vec2, 4> points = {{math::vec2(dst.x, dst.y),
                                               math::vec2(dst.x + dst.w, dst.y),
                                               math::vec2(dst.x + dst.w, dst.y + dst.h),
                                               math::vec2(dst.x, dst.y + dst.h)}};

    add_rect(points, col, filled, thickness);
}

void draw_list::add_rect(const rect& dst, const math::transformf& transform, const color& col, bool filled,
                         float thickness)
{
    add_rect(frect(dst.x, dst.y, dst.w, dst.h), transform, col, filled, thickness);
}

void draw_list::add_rect(const frect& dst, const math::transformf& transform, const color& col, bool filled,
                         float thickness)
{
    // If we want this to be batched with other calls we need to
    // do the transformations on the cpu side
    std::array<math::vec2, 4> points = {{math::vec2(dst.x, dst.y),
                                         math::vec2(dst.x + dst.w, dst.y),
                                         math::vec2(dst.x + dst.w, dst.y + dst.h),
                                         math::vec2(dst.x, dst.y + dst.h)}};
    for(auto& p : points)
    {
        p = transform.transform_coord(p);
    }


    add_rect(points, col, filled, thickness);
}

void draw_list::add_line(const math::vec2& start, const math::vec2& end, const color& col, float thickness)
{
    polyline line;
    line.line_to(start);
    line.line_to(end);
    add_polyline(line, col, false, thickness);
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
    constexpr auto type = primitive_type::triangles;
    const auto index_offset = std::uint32_t(vertices.size());
    const auto vertices_added = add_rect_primitive(vertices, points, col, min_uv, max_uv);
    if(setup.program.shader)
    {
        detail::add_indices(*this, vertices_added, index_offset, type, setup);
    }
    else
    {
        if(texture)
        {
            program_setup program{};
            program.program = multi_channel_texture_program();

            uint64_t hash{0};
            utils::hash(hash, texture);
            program.uniforms_hash = hash;

            hash = detail::calc_final_hash(program.uniforms_hash, type, program);
            if(!detail::can_be_batched(*this, hash))
            {
                program.begin = [texture](const gpu_context& ctx) {
                    ctx.program.shader->set_uniform("uTexture", texture);
                };
            }

            detail::add_indices(*this, vertices_added, index_offset, type, std::move(program));
        }
        else
        {
            detail::add_indices(*this, vertices_added, index_offset, type, get_simple_setup());
        }
    }
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
    int sdf_spread = font->sdf_spread;
    float scale = std::max(transform.get_scale().x, transform.get_scale().y);
    float distance_field_multiplier = (2 * sdf_spread + 1) * unit_length_in_pixels_at_font_position * scale;
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
                uint64_t hash{0};
                utils::hash(hash,
                            distance_field_multiplier,
                            outline_width,
                            outline_color,
                            texture);

                program.uniforms_hash = hash;
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
                uint64_t hash{0};
                utils::hash(hash, texture);
                program.uniforms_hash = hash;
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
        }
        add_vertices(transformed_geometry, primitive_type::triangles, texture, program);
    }
    else
    {
        program.end = [](const gpu_context& ctx)
        {
            ctx.rend.pop_transform();
        };

        add_vertices(geometry, primitive_type::triangles, texture, program);
    }
}

void draw_list::add_text_superscript(const text& text_whole, const text& text_partial,
                                     const math::transformf& transform, text::alignment align,
                                     float partial_scale, const program_setup& setup)
{
    math::transformf transform_whole;
    math::transformf transform_partial;
    transform_partial.scale({partial_scale, partial_scale, 1.0f});

    add_text_superscript_impl(text_whole, text_partial, transform, transform_whole, transform_partial, align,
                              partial_scale, setup);
}

void draw_list::add_text_superscript(const text& whole_text, const text& partial_text,
                                     const math::transformf& transform, const rect& dst_rect,
                                     text::alignment align, float partial_scale, size_fit sz_fit,
                                     dimension_fit dim_fit, const program_setup& setup)
{
    math::transformf transform_whole;
    math::transformf transform_partial;

    transform_partial.scale({partial_scale, partial_scale, 1.0f});

    auto scale_whole = transform_whole.get_scale() * transform.get_scale();
    auto scale_partial = transform_partial.get_scale() * transform.get_scale();
    float text_width = whole_text.get_width() * scale_whole.x + partial_text.get_width() * scale_partial.x;
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

    add_text_superscript_impl(whole_text, partial_text, transform * scale_trans, transform_whole,
                              transform_partial, align, partial_scale, setup);
}

void draw_list::add_text_superscript_impl(const text& text_whole, const text& text_partial,
                                          const math::transformf& transform,
                                          math::transformf& transform_whole,
                                          math::transformf& transform_partial, text::alignment align,
                                          float partial_scale, const program_setup& setup)
{
    if(text_whole.get_lines().size() > 1)
    {
        log("Superscript text should not be multiline. This api will not behave properly.");
    }
    const auto text_whole_width = text_whole.get_width();

    const auto text_whole_min_baseline_height = text_whole.get_utf8_text().empty() ?
                                                text_partial.get_min_baseline_height() :
                                                text_whole.get_min_baseline_height();
    const auto text_whole_max_baseline_height = text_whole.get_utf8_text().empty() ?
                                                text_partial.get_max_baseline_height() :
                                                text_whole.get_max_baseline_height();

    const auto text_partial_width = text_partial.get_width() * partial_scale;
    const auto text_parital_height = text_whole.get_utf8_text().empty() ?
                                     text_partial.get_height() * (1.0f - partial_scale) :
                                     text_whole.get_height() - (text_partial.get_height() * partial_scale);

    const auto text_partial_min_baseline_height =
        text_partial.get_min_baseline_height() * partial_scale;
    const auto text_partial_max_baseline_height =
        text_partial.get_max_baseline_height() * partial_scale;

    const auto half_text_whole_width = text_whole_width * 0.5f;
    const auto half_text_partial_width = text_partial_width * 0.5f;
    const auto half_text_parital_height = text_parital_height * 0.5f;

    const auto text_whole_shadow_height = text_whole.get_utf8_text().empty() ?
                                          text_partial.get_shadow_offsets().y :
                                          text_whole.get_shadow_offsets().y;
    const auto text_partial_shadow_height = text_partial.get_shadow_offsets().y * partial_scale;
    const auto shadows_height_dif = text_whole_shadow_height - text_partial_shadow_height;
    const auto half_shadows_height_dif = shadows_height_dif * 0.5f;

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
            transform_partial.translate(
                {0.0f, shadows_height_dif -
                 (text_whole_min_baseline_height - text_partial_min_baseline_height), 0.0f});
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
            transform_partial.translate({half_text_whole_width,
                                         shadows_height_dif -
                                         (text_whole_min_baseline_height - text_partial_min_baseline_height),
                                         0.0f});
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
            transform_partial.translate(
                {0.0f, shadows_height_dif -
                 (text_whole_min_baseline_height - text_partial_min_baseline_height), 0.0f});
        }
        break;

        case text::alignment::left:
        {
            transform_partial.translate({text_whole_width, half_shadows_height_dif - half_text_parital_height, 0.0f});
        }
        break;

        case text::alignment::center:
        {
            transform_whole.translate({-half_text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({half_text_whole_width, half_shadows_height_dif - half_text_parital_height, 0.0f});
        }
        break;

        case text::alignment::right:
        {
            transform_whole.translate({-text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({0.0f, half_shadows_height_dif - half_text_parital_height, 0.0f});
        }
        break;

        case text::alignment::bottom_left:
        {
            transform_partial.translate({text_whole_width, shadows_height_dif - text_parital_height, 0.0f});
        }
        break;
        case text::alignment::baseline_bottom_left:
        {
            transform_partial.translate({text_whole_width,
                                         shadows_height_dif -
                                         (text_whole_max_baseline_height - text_partial_max_baseline_height),
                                         0.0f});
        }
        break;

        case text::alignment::bottom:
        {
            transform_whole.translate({-half_text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({half_text_whole_width, shadows_height_dif - text_parital_height, 0.0f});
        }
        break;
        case text::alignment::baseline_bottom:
        {
            transform_whole.translate({-half_text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({half_text_whole_width,
                                         shadows_height_dif -
                                         (text_whole_max_baseline_height - text_partial_max_baseline_height),
                                         0.0f});
        }
        break;

        case text::alignment::bottom_right:
        {
            transform_whole.translate({-text_partial_width, 0.0f, 0.0f});
            transform_partial.translate({0.0f, shadows_height_dif - text_parital_height, 0.0f});
        }
        break;
        case text::alignment::baseline_bottom_right:
        {
            transform_whole.translate({-text_partial_width, 0.0f, 0.0f});
            transform_partial.translate(
                {0.0f, shadows_height_dif -
                 (text_whole_max_baseline_height - text_partial_max_baseline_height), 0.0f});
        }
        break;
        default:
            break;
    }
    add_text(text_whole, transform * transform_whole, setup);
    add_text(text_partial, transform * transform_partial, setup);
}

void draw_list::add_text_subscript(const text& whole_text,
                                   const text& partial_text,
                                   const math::transformf& transform,
                                   text::alignment align,
                                   float partial_scale,
                                   const program_setup& setup)
{
    math::transformf transform_whole;
    math::transformf transform_partial;
    transform_partial.scale({partial_scale, partial_scale, 1.0f});

    add_text_subscript_impl(whole_text, partial_text, transform, transform_whole, transform_partial, align,
                            partial_scale, setup);
}

void draw_list::add_text_subscript(const text& whole_text,
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
    float text_width = whole_text.get_width() * scale_whole.x + partial_text.get_width() * scale_partial.x;
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

    add_text_subscript_impl(whole_text, partial_text, transform * scale_trans, transform_whole,
                            transform_partial, align, partial_scale, setup);
}

void draw_list::add_text_subscript_impl(const text& whole_text,
                                        const text& partial_text,
                                        const math::transformf& transform,
                                        math::transformf& whole_transform,
                                        math::transformf& partial_transform,
                                        text::alignment align,
                                        float partial_scale,
                                        const program_setup& setup)
{
    if(whole_text.get_lines().size() > 1)
    {
        log("Subscript text should not be multiline. This api will not behave properly.");
    }

    const auto text_whole_width = whole_text.get_width();

    const auto text_whole_height = whole_text.get_utf8_text().empty() ?
                                   partial_text.get_height() :
                                   whole_text.get_height();

    const auto text_whole_shadow_height = whole_text.get_utf8_text().empty() ?
                                          partial_text.get_shadow_offsets().y :
                                          whole_text.get_shadow_offsets().y;

    const auto text_whole_max_baseline_height = whole_text.get_utf8_text().empty() ?
                                                partial_text.get_max_baseline_height() :
                                                whole_text.get_max_baseline_height();

    const auto text_partial_width = partial_text.get_width() * partial_scale;
    const auto text_parital_height = partial_scale * partial_text.get_height();
    const auto text_parital_height_dif = text_whole_height - text_parital_height;

    const auto text_partial_shadow_height = partial_text.get_shadow_offsets().y * partial_scale;
    const auto shadows_height_dif = text_whole_shadow_height - text_partial_shadow_height;
    const auto half_shadows_height_dif = shadows_height_dif * 0.5f;

    const auto text_partial_max_baseline_height =
        partial_text.get_max_baseline_height() * partial_scale;

    const auto half_text_whole_width = text_whole_width * 0.5f;
    const auto half_text_partial_width = text_partial_width * 0.5f;
    const auto half_text_parital_height_dif = text_parital_height_dif * 0.5f;

    const auto top_y_aligns_parial_offset = text_whole_max_baseline_height - text_partial_max_baseline_height - shadows_height_dif;
    const auto center_y_aligns_parial_offset = text_whole_max_baseline_height - (half_text_parital_height_dif + text_partial_max_baseline_height) - half_shadows_height_dif;
    const auto bottom_y_aligns_parial_offset = - ((text_whole_height - text_whole_max_baseline_height) -
                                                  (text_parital_height - text_partial_max_baseline_height));

    switch(align)
    {
        case text::alignment::top_left:
        {
            partial_transform.translate({text_whole_width, top_y_aligns_parial_offset, 0.0f});
        }
        break;
        case text::alignment::baseline_top_left:
        {
            partial_transform.translate({text_whole_width, 0.0f, 0.0f});
        }
        break;

        case text::alignment::top:
        {
            whole_transform.translate({-half_text_partial_width, 0.0f, 0.0f});
            partial_transform.translate({half_text_whole_width, top_y_aligns_parial_offset, 0.0f});
        }
        break;
        case text::alignment::baseline_top:
        {
            whole_transform.translate({-half_text_partial_width, 0.0f, 0.0f});
            partial_transform.translate({half_text_whole_width, 0.0f, 0.0f});
        }
        break;

        case text::alignment::top_right:
        {
            whole_transform.translate({-text_partial_width, 0.0f, 0.0f});
            partial_transform.translate({0.0f, top_y_aligns_parial_offset, 0.0f});
        }
        break;
        case text::alignment::baseline_top_right:
        {
            whole_transform.translate({-text_partial_width, 0.0f, 0.0f});
        }
        break;

        case text::alignment::left:
        {
            partial_transform.translate({text_whole_width,
                                         center_y_aligns_parial_offset,
                                         0.0f});
        }
        break;

        case text::alignment::center:
        {
            whole_transform.translate({-half_text_partial_width, 0.0f, 0.0f});
            partial_transform.translate({half_text_whole_width,
                                         center_y_aligns_parial_offset,
                                         0.0f});
        }
        break;

        case text::alignment::right:
        {
            whole_transform.translate({-text_partial_width, 0.0f, 0.0f});
            partial_transform.translate({0.0f, center_y_aligns_parial_offset, 0.0f});
        }
        break;

        case text::alignment::bottom_left:
        {
            partial_transform.translate({text_whole_width, bottom_y_aligns_parial_offset, 0.0f});
        }
        break;
        case text::alignment::baseline_bottom_left:
        {
            partial_transform.translate({text_whole_width, 0.0f, 0.0f});
        }
        break;

        case text::alignment::bottom:
        {
            whole_transform.translate({-half_text_partial_width, 0.0f, 0.0f});
            partial_transform.translate({half_text_whole_width, bottom_y_aligns_parial_offset, 0.0f});
        }
        break;
        case text::alignment::baseline_bottom:
        {
            whole_transform.translate({-half_text_partial_width, 0.0f, 0.0f});
            partial_transform.translate({half_text_whole_width, 0.0f, 0.0f});
        }
        break;

        case text::alignment::bottom_right:
        {
            whole_transform.translate({-text_partial_width, 0.0f, 0.0f});
            partial_transform.translate({0.0f, bottom_y_aligns_parial_offset, 0.0f});
        }
        break;
        case text::alignment::baseline_bottom_right:
        {
            whole_transform.translate({-text_partial_width, 0.0f, 0.0f});
        }
        break;
        default:
            break;
    }

    add_text(whole_text, transform * whole_transform, setup);
    add_text(partial_text, transform * partial_transform, setup);
};


void draw_list::add_vertices(const std::vector<vertex_2d>& verts, const primitive_type type,
                             const texture_view& texture, const program_setup& setup)
{
    add_vertices(verts.data(), verts.size(), type, texture, setup);
}

void draw_list::add_vertices(const vertex_2d* verts, size_t count, primitive_type type,
                             const texture_view& texture, const program_setup& setup)
{
    if(count == 0)
    {
        return;
    }
    const auto index_offset = std::uint32_t(vertices.size());
    vertices.resize(index_offset + count);
    std::memcpy(&vertices[index_offset], verts, sizeof(vertex_2d) * count);

    if(setup.program.shader)
    {
        detail::add_indices(*this, std::uint32_t(count), index_offset, type, setup);
    }
    else
    {
        if(texture)
        {
            program_setup program{};
            program.program = multi_channel_texture_program();

            uint64_t hash{0};
            utils::hash(hash, texture);
            program.uniforms_hash = hash;

            hash = detail::calc_final_hash(program.uniforms_hash, type, program);
            if(!detail::can_be_batched(*this, hash))
            {
                program.begin = [texture](const gpu_context& ctx)
                {
                    ctx.program.shader->set_uniform("uTexture", texture);
                };
            }
            detail::add_indices(*this, std::uint32_t(count), index_offset, type, std::move(program));
        }
        else
        {
            detail::add_indices(*this, std::uint32_t(count), index_offset, type, get_simple_setup());
        }
    }
}

void normalize2f_over_zero(float& vx, float& vy)
{
    float d2 = vx*vx + vy*vy;
    if (d2 > 0.0f)
    {
        float inv_len = 1.0f / math::sqrt(d2);
        vx *= inv_len;
        vy *= inv_len;
    }
}
void fixnormal2f(float& vx, float& vy)
{
    float d2 = vx*vx + vy*vy;
    float inv_lensq = 1.0f / d2;
    vx *= inv_lensq;
    vy *= inv_lensq;
}

color get_vertical_gradient(const color& ct,const color& cb, float DH, float H)
{
    const float fa = DH/H;
    const float fc = (1.f-fa);
    return {
        uint8_t(float(ct.r) * fc + float(cb.r) * fa),
        uint8_t(float(ct.g) * fc + float(cb.g) * fa),
        uint8_t(float(ct.b) * fc + float(cb.b) * fa),
        uint8_t(float(ct.a) * fc + float(cb.a) * fa)
    };
}

void draw_list::add_polyline(const polyline& poly, const color& col, bool closed, float thickness, float antialias_size)
{
    add_polyline_gradient(poly, col, col, closed, thickness, antialias_size);
}


void draw_list::add_polyline_gradient(const polyline& poly, const color& coltop, const color& colbot, bool closed, float thickness, float antialias_size)
{
    detail::add_indices(*this, 0, 0, primitive_type::triangles, get_simple_setup());

    const auto& points = poly.get_points();
    auto points_count = points.size();

    if (points_count < 2)
        return;

    size_t count = points_count;
    if (!closed)
        count = points_count-1;

    const bool thick_line = thickness > 1.0f;
    if (antialias_size > 0.0f)
    {
        // Anti-aliased stroke
        const float aa_size = antialias_size;
        color coltop_trans = coltop;
        coltop_trans.a = 0;
        color colbot_trans = colbot;
        colbot_trans.a = 0;
        const size_t idx_count = thick_line ? count*18 : count*12;
        const size_t vtx_count = thick_line ? points_count*4 : points_count*3;

        size_t idx_current_idx{};
        size_t vtx_current_idx{};
        detail::prim_reserve(*this, idx_count, vtx_count, idx_current_idx, vtx_current_idx);

        auto idx_write_ptr = &indices[idx_current_idx];
        auto vtx_write_ptr = &vertices[vtx_current_idx];


        // Temporary buffer
        std::vector<math::vec2> temp_normals(points_count * (thick_line ? 5 : 3));
        math::vec2* temp_points = temp_normals.data() + points_count;

        for (size_t i1 = 0; i1 < count; i1++)
        {
            const size_t i2 = (i1+1) == points_count ? 0 : i1+1;
            float dx = points[i2].x - points[i1].x;
            float dy = points[i2].y - points[i1].y;
            normalize2f_over_zero(dx, dy);
            temp_normals[i1].x = dy;
            temp_normals[i1].y = -dx;

        }
        if (!closed)
            temp_normals[points_count-1] = temp_normals[points_count-2];

        if (!thick_line)
        {
            if (!closed)
            {
                temp_points[0] = points[0] + temp_normals[0] * aa_size;
                temp_points[1] = points[0] - temp_normals[0] * aa_size;
                temp_points[(points_count-1)*2+0] = points[points_count-1] + temp_normals[points_count-1] * aa_size;
                temp_points[(points_count-1)*2+1] = points[points_count-1] - temp_normals[points_count-1] * aa_size;
            }

            // FIXME-OPT: Merge the different loops, possibly remove the temporary buffer.
            size_t idx1 = vtx_current_idx;
            for (size_t i1 = 0; i1 < count; i1++)
            {
                const size_t i2 = (i1+1) == points_count ? 0 : i1+1;
                size_t idx2 = (i1+1) == points_count ? vtx_current_idx : idx1+3;

                // Average normals
                float dm_x = (temp_normals[i1].x + temp_normals[i2].x) * 0.5f;
                float dm_y = (temp_normals[i1].y + temp_normals[i2].y) * 0.5f;
                fixnormal2f(dm_x, dm_y);
                dm_x *= aa_size;
                dm_y *= aa_size;

                // Add temporary vertexes
                auto* out_vtx = &temp_points[i2*2];
                out_vtx[0].x = points[i2].x + dm_x;
                out_vtx[0].y = points[i2].y + dm_y;
                out_vtx[1].x = points[i2].x - dm_x;
                out_vtx[1].y = points[i2].y - dm_y;

                // Add indexes
                idx_write_ptr[0] = index_t(idx2+0); idx_write_ptr[1] = index_t(idx1+0); idx_write_ptr[2] = index_t(idx1+2);
                idx_write_ptr[3] = index_t(idx1+2); idx_write_ptr[4] = index_t(idx2+2); idx_write_ptr[5] = index_t(idx2+0);
                idx_write_ptr[6] = index_t(idx2+1); idx_write_ptr[7] = index_t(idx1+1); idx_write_ptr[8] = index_t(idx1+0);
                idx_write_ptr[9] = index_t(idx1+0); idx_write_ptr[10]= index_t(idx2+0); idx_write_ptr[11]= index_t(idx2+1);
                idx_write_ptr += 12;

                idx1 = idx2;
            }

            // Add vertexes
            for (size_t i = 0; i < points_count; i++)
            {
                vtx_write_ptr[0].pos = points[i];          vtx_write_ptr[0].col = coltop;
                vtx_write_ptr[1].pos = temp_points[i*2+0]; vtx_write_ptr[1].col = coltop_trans;
                vtx_write_ptr[2].pos = temp_points[i*2+1]; vtx_write_ptr[2].col = colbot_trans;
                vtx_write_ptr += 3;
            }
        }
        else
        {
            const float half_inner_thickness = (thickness - aa_size) * 0.5f;
            if (!closed)
            {
                temp_points[0] = points[0] + temp_normals[0] * (half_inner_thickness + aa_size);
                temp_points[1] = points[0] + temp_normals[0] * (half_inner_thickness);
                temp_points[2] = points[0] - temp_normals[0] * (half_inner_thickness);
                temp_points[3] = points[0] - temp_normals[0] * (half_inner_thickness + aa_size);
                temp_points[(points_count-1)*4+0] = points[points_count-1] + temp_normals[points_count-1] * (half_inner_thickness + aa_size);
                temp_points[(points_count-1)*4+1] = points[points_count-1] + temp_normals[points_count-1] * (half_inner_thickness);
                temp_points[(points_count-1)*4+2] = points[points_count-1] - temp_normals[points_count-1] * (half_inner_thickness);
                temp_points[(points_count-1)*4+3] = points[points_count-1] - temp_normals[points_count-1] * (half_inner_thickness + aa_size);
            }

            // FIXME-OPT: Merge the different loops, possibly remove the temporary buffer.
            size_t idx1 = vtx_current_idx;
            for (size_t i1 = 0; i1 < count; i1++)
            {
                const size_t i2 = (i1+1) == points_count ? 0 : i1+1;
                size_t idx2 = (i1+1) == points_count ? vtx_current_idx : idx1+4;

                // Average normals
                float dm_x = (temp_normals[i1].x + temp_normals[i2].x) * 0.5f;
                float dm_y = (temp_normals[i1].y + temp_normals[i2].y) * 0.5f;
                fixnormal2f(dm_x, dm_y);
                float dm_out_x = dm_x * (half_inner_thickness + aa_size);
                float dm_out_y = dm_y * (half_inner_thickness + aa_size);
                float dm_in_x = dm_x * half_inner_thickness;
                float dm_in_y = dm_y * half_inner_thickness;

                // Add temporary vertexes
                auto* out_vtx = &temp_points[i2*4];
                out_vtx[0].x = points[i2].x + dm_out_x;
                out_vtx[0].y = points[i2].y + dm_out_y;
                out_vtx[1].x = points[i2].x + dm_in_x;
                out_vtx[1].y = points[i2].y + dm_in_y;
                out_vtx[2].x = points[i2].x - dm_in_x;
                out_vtx[2].y = points[i2].y - dm_in_y;
                out_vtx[3].x = points[i2].x - dm_out_x;
                out_vtx[3].y = points[i2].y - dm_out_y;

                // Add indexes
                idx_write_ptr[0]  = index_t(idx2+1); idx_write_ptr[1]  = index_t(idx1+1); idx_write_ptr[2]  = index_t(idx1+2);
                idx_write_ptr[3]  = index_t(idx1+2); idx_write_ptr[4]  = index_t(idx2+2); idx_write_ptr[5]  = index_t(idx2+1);
                idx_write_ptr[6]  = index_t(idx2+1); idx_write_ptr[7]  = index_t(idx1+1); idx_write_ptr[8]  = index_t(idx1+0);
                idx_write_ptr[9]  = index_t(idx1+0); idx_write_ptr[10] = index_t(idx2+0); idx_write_ptr[11] = index_t(idx2+1);
                idx_write_ptr[12] = index_t(idx2+2); idx_write_ptr[13] = index_t(idx1+2); idx_write_ptr[14] = index_t(idx1+3);
                idx_write_ptr[15] = index_t(idx1+3); idx_write_ptr[16] = index_t(idx2+3); idx_write_ptr[17] = index_t(idx2+2);
                idx_write_ptr += 18;

                idx1 = idx2;
            }

            // Add vertexes
            for (size_t i = 0; i < points_count; i++)
            {
                vtx_write_ptr[0].pos = temp_points[i*4+0]; vtx_write_ptr[0].col = coltop_trans;
                vtx_write_ptr[1].pos = temp_points[i*4+1]; vtx_write_ptr[1].col = coltop;
                vtx_write_ptr[2].pos = temp_points[i*4+2]; vtx_write_ptr[2].col = colbot;
                vtx_write_ptr[3].pos = temp_points[i*4+3]; vtx_write_ptr[3].col = colbot_trans;

                vtx_write_ptr += 4;
            }
        }
    }
    else
    {
        // Non Anti-aliased Stroke
        const size_t idx_count = count*6;
        const size_t vtx_count = count*4;      // FIXME-OPT: Not sharing edges
        size_t idx_current_idx{};
        size_t vtx_current_idx{};
        detail::prim_reserve(*this, idx_count, vtx_count, idx_current_idx, vtx_current_idx);

        auto idx_write_ptr = &indices[idx_current_idx];
        auto vtx_write_ptr = &vertices[vtx_current_idx];

        for (size_t i1 = 0; i1 < count; i1++)
        {
            const size_t i2 = (i1+1) == points_count ? 0 : i1+1;
            const auto& p1 = points[i1];
            const auto& p2 = points[i2];

            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            normalize2f_over_zero(dx, dy);
            dx *= (thickness * 0.5f);
            dy *= (thickness * 0.5f);

            vtx_write_ptr[0].pos.x = p1.x + dy; vtx_write_ptr[0].pos.y = p1.y - dx; vtx_write_ptr[0].col = coltop;
            vtx_write_ptr[1].pos.x = p2.x + dy; vtx_write_ptr[1].pos.y = p2.y - dx; vtx_write_ptr[1].col = coltop;
            vtx_write_ptr[2].pos.x = p2.x - dy; vtx_write_ptr[2].pos.y = p2.y + dx; vtx_write_ptr[2].col = colbot;
            vtx_write_ptr[3].pos.x = p1.x - dy; vtx_write_ptr[3].pos.y = p1.y + dx; vtx_write_ptr[3].col = colbot;
            vtx_write_ptr += 4;

            idx_write_ptr[0] = index_t(vtx_current_idx); idx_write_ptr[1] = index_t(vtx_current_idx+1); idx_write_ptr[2] = index_t(vtx_current_idx+2);
            idx_write_ptr[3] = index_t(vtx_current_idx); idx_write_ptr[4] = index_t(vtx_current_idx+2); idx_write_ptr[5] = index_t(vtx_current_idx+3);
            idx_write_ptr += 6;
            vtx_current_idx += 4;
        }
    }
}


void draw_list::add_polyline_filled_convex(const polyline& poly, const color& col, float antialias_size)
{
    add_polyline_filled_convex_gradient(poly, col, col, antialias_size);
}

void draw_list::add_polyline_filled_convex_gradient(const polyline& poly, const color& coltop, const color& colbot, float antialias_size)
{
    detail::add_indices(*this, 0, 0, primitive_type::triangles, get_simple_setup());

    const auto& points = poly.get_points();
    auto points_count = points.size();

    if (points_count < 3)
        return;

    float miny = 999999999999999999;
    float maxy = -miny;

    float minx = 999999999999999999;
    float maxx = -minx;

    for (size_t i = 0; i < points_count; i++)
    {
        const float w = points[i].x;
        const float h = points[i].y;
        if (h < miny) miny = h;
        else if (h > maxy) maxy = h;

        if (w < minx) minx = w;
        else if (w > maxx) maxx = w;
    }

    float height = maxy-miny;
    //float width = maxx-minx;

    if (antialias_size > 0.0f)
    {
        // Anti-aliased Fill
        const float aa_size = antialias_size;
        color coltop_trans = coltop;
        coltop_trans.a = 0;
        color colbot_trans = colbot;
        colbot_trans.a = 0;

        const size_t idx_count = (points_count-2)*3 + points_count*6;
        const size_t vtx_count = (points_count*2);

        size_t idx_current_idx{};
        size_t vtx_current_idx{};
        detail::prim_reserve(*this, idx_count, vtx_count, idx_current_idx, vtx_current_idx);

        auto idx_write_ptr = &indices[idx_current_idx];
        auto vtx_write_ptr = &vertices[vtx_current_idx];

        // Add indexes for fill
        size_t vtx_inner_idx = vtx_current_idx;
        size_t vtx_outer_idx = vtx_current_idx+1;
        for (size_t i = 2; i < points_count; i++)
        {
            idx_write_ptr[0] = index_t(vtx_inner_idx); idx_write_ptr[1] = index_t(vtx_inner_idx+((i-1)<<1)); idx_write_ptr[2] = index_t(vtx_inner_idx+(i<<1));
            idx_write_ptr += 3;
        }

        // Compute normals
        std::vector<math::vec2> temp_normals(points_count);

        for (size_t i0 = points_count-1, i1 = 0; i1 < points_count; i0 = i1++)
        {
            const auto& p0 = points[i0];
            const auto& p1 = points[i1];
            float dx = p1.x - p0.x;
            float dy = p1.y - p0.y;
            normalize2f_over_zero(dx, dy);
            temp_normals[i0].x = dy;
            temp_normals[i0].y = -dx;
        }

        for (size_t i0 = points_count-1, i1 = 0; i1 < points_count; i0 = i1++)
        {
            // Average normals
            const auto& n0 = temp_normals[i0];
            const auto& n1 = temp_normals[i1];
            float dm_x = (n0.x + n1.x) * 0.5f;
            float dm_y = (n0.y + n1.y) * 0.5f;
            fixnormal2f(dm_x, dm_y);
            dm_x *= aa_size * 0.5f;
            dm_y *= aa_size * 0.5f;

            // Add vertices
            vtx_write_ptr[0].pos.x = (points[i1].x - dm_x);
            vtx_write_ptr[0].pos.y = (points[i1].y - dm_y);

            vtx_write_ptr[1].pos.x = (points[i1].x + dm_x);
            vtx_write_ptr[1].pos.y = (points[i1].y + dm_y);

            vtx_write_ptr[0].col = get_vertical_gradient(coltop, colbot, points[i1].y-miny, height);              // Inner
            vtx_write_ptr[1].col = get_vertical_gradient(coltop_trans, colbot_trans, points[i1].y-miny, height);  // Outer
            vtx_write_ptr += 2;

            // Add indexes for fringes
            idx_write_ptr[0] = index_t(vtx_inner_idx+(i1<<1)); idx_write_ptr[1] = index_t(vtx_inner_idx+(i0<<1)); idx_write_ptr[2] = index_t(vtx_outer_idx+(i0<<1));
            idx_write_ptr[3] = index_t(vtx_outer_idx+(i0<<1)); idx_write_ptr[4] = index_t(vtx_outer_idx+(i1<<1)); idx_write_ptr[5] = index_t(vtx_inner_idx+(i1<<1));
            idx_write_ptr += 6;
        }
    }
    else
    {
        // Non Anti-aliased Fill
        const size_t idx_count = (points_count-2)*3;
        const size_t vtx_count = points_count;
        size_t idx_current_idx{};
        size_t vtx_current_idx{};
        detail::prim_reserve(*this, idx_count, vtx_count, idx_current_idx, vtx_current_idx);

        auto idx_write_ptr = &indices[idx_current_idx];
        auto vtx_write_ptr = &vertices[vtx_current_idx];
        for (size_t i = 0; i < vtx_count; i++)
        {
            vtx_write_ptr[0].pos = points[i];
            vtx_write_ptr[0].col = get_vertical_gradient(coltop, colbot, vtx_write_ptr[0].pos.y-miny,height);

            vtx_write_ptr++;
        }
        for (size_t i = 2; i < points_count; i++)
        {
            idx_write_ptr[0] = index_t(vtx_current_idx); idx_write_ptr[1] = index_t(vtx_current_idx+i-1); idx_write_ptr[2] = index_t(vtx_current_idx+i);
            idx_write_ptr += 3;
        }
    }
}

void draw_list::add_ellipse(const math::vec2& center, const math::vec2& radii, const color& col, size_t num_segments, float thickness)
{
    add_ellipse_gradient(center, radii, col, col, num_segments, thickness);

}

void draw_list::add_ellipse_gradient(const math::vec2& center, const math::vec2& radii, const color& col1, const color& col2, size_t num_segments, float thickness)
{
    if ((col1.a == 0 && col2.a == 0) || num_segments <= 2)
        return;

    polyline line;
    line.ellipse(center, radii, num_segments);
    add_polyline_gradient(line, col1, col2, true, thickness);

}

void draw_list::add_ellipse_filled(const math::vec2& center, const math::vec2& radii, const color& col, size_t num_segments)
{
    if (col.a == 0|| num_segments <= 2)
        return;

    polyline line;
    line.ellipse(center, radii, num_segments);
    add_polyline_filled_convex(line, col, true);
}

void draw_list::add_bezier_curve(const math::vec2& pos0, const math::vec2& cp0, const math::vec2& cp1, const math::vec2& pos1, const color& col, float thickness, int num_segments)
{
    if (col.a == 0)
        return;

    polyline line;
    line.line_to(pos0);
    line.bezier_curve_to(cp0, cp1, pos1, num_segments);
    add_polyline(line, col, false, thickness);
}

void draw_list::add_curved_path_gradient(const std::vector<math::vec2>& points, const color& col1, const color& col2, float thickness, float antialias_size)
{
    if((col1.a == 0 && col2.a == 0) || points.size() < 2)
        return;

    float radius = 0.5f * thickness;
    polyline front_cap;
    front_cap.ellipse(points.front(), {radius + 0.5f, radius + 0.5f});
    add_polyline_filled_convex_gradient(front_cap, col1, col2, antialias_size);

    polyline back_cap;
    back_cap.ellipse(points.back(), {radius + 0.5f, radius + 0.5f});
    add_polyline_filled_convex_gradient(back_cap, col1, col2, antialias_size);

    polyline line;
    line.path(points, radius);
    add_polyline_gradient(line, col1, col2, false, thickness, antialias_size);
}

void draw_list::push_clip(const rect& clip)
{
    clip_rects.emplace_back(clip);
}

void draw_list::pop_clip()
{
    if(!clip_rects.empty())
    {
        clip_rects.pop_back();
    }
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
        }

        for(size_t i = 0; i < geometry.size(); i+=4)
        {
            polyline line;
            color col{};
            for(size_t j = 0; j < 4; ++j)
            {
                col = geometry[i + j].col;
                line.line_to(geometry[i + j].pos);
            }
            add_polyline(line, col, true);
        }

    }

    auto rect = t.get_frect();
    add_rect(rect, transform, color::red(), false, 1.0f);
    const auto& lines = t.get_lines_metrics();
    {
        auto col = color::cyan();
        std::string desc = "ascent ";
        for(const auto& line : lines)
        {
            auto v1 = transform.transform_coord({line.minx, line.ascent});
            auto v2 = transform.transform_coord({line.maxx, line.ascent});

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
        for(const auto& line : lines)
        {
            auto v1 = transform.transform_coord({line.minx, line.baseline});
            auto v2 = transform.transform_coord({line.maxx, line.baseline});

            add_line(v1, v2, col);

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
        for(const auto& line : lines)
        {
            auto v1 = transform.transform_coord({line.minx, line.descent});
            auto v2 = transform.transform_coord({line.maxx, line.descent});

            add_line(v1, v2, col, 1.0f);

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
        for(const auto& line : lines)
        {
            auto line_height = t.get_font()->line_height;
            auto v1 = transform.transform_coord({line.maxx, line.ascent});
            auto v2 = transform.transform_coord({line.maxx, line.ascent + line_height});

            add_line(v1, v2, col);

        }
    }

}


}
