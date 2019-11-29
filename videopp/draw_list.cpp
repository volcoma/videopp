#include "draw_list.h"
#include <cmath>
#include <codecvt>
#include <locale>
#include <sstream>
#include <iomanip>

#include "font.h"
#include "logger.h"
#include "renderer.h"
#include "text.h"

namespace gfx
{

namespace
{
bool debug_draw = false;


template<typename T>
inline std::array<math::vec2, 4> transform_rect(const rect_t<T>& r) noexcept
{
   return {{math::vec2(r.x, r.y),
            math::vec2(r.x + r.w, r.y),
            math::vec2(r.x + r.w, r.y + r.h),
            math::vec2(r.x, r.y + r.h)}};
}

template<typename T>
std::array<math::vec2, 4> transform_rect(const rect_t<T>& r, const math::transformf& transform) noexcept
{
    auto points = transform_rect(r);

    for(auto& p : points)
    {
        p = transform.transform_coord(p);
    }

    return points;
}


inline uint64_t simple_hash() noexcept
{
    uint64_t seed{0};
    utils::hash(seed, 0.0f);
    return seed;
}

inline bool can_be_batched(draw_list& list, uint64_t hash) noexcept
{
    return !list.commands.empty() && list.commands.back().hash == hash;
}

inline void transform_vertices(draw_list& list, size_t vtx_offset, size_t vtx_count)
{
    if(!list.transforms.empty())
    {
        const auto& tr = list.transforms.back();
        for(size_t i = vtx_offset; i < vtx_offset + vtx_count; ++i)
        {
            auto& v = list.vertices[i];
            v.pos = tr.transform_coord(v.pos);
        }
    }
}

template<typename Setup>
inline draw_cmd& add_cmd_impl(draw_list& list, draw_type dr_type, uint32_t vertices_before, uint32_t vertices_added,
                              uint32_t indices_before, uint32_t indices_added, primitive_type type, blending_mode deduced_blend, Setup&& setup)
{
    if(indices_added == 0)
    {
        if(dr_type == draw_type::elements)
        {
            auto index_offset = vertices_before;
            switch(type)
            {
                case primitive_type::triangles:
                {
                    const uint32_t rect_vertices = 4;
                    const uint32_t rects = vertices_added / rect_vertices;
                    // indices added will be (rects * (rect_vertices - 2)) * 3;
                    list.indices.resize(indices_before + (rects * (rect_vertices - 2)) * 3);
                    for(uint32_t r = 0; r < rects; ++r)
                    {
                        for(uint32_t i = 2; i < rect_vertices; ++i)
                        {
                            draw_list::index_t src_indices[] = {index_offset, index_offset + i - 1, index_offset + i};
                            std::memcpy(list.indices.data() + indices_before + indices_added, src_indices, sizeof(src_indices));
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
                        list.indices.resize(indices_before + (vertices_added - 1) * 2);

                        for(uint32_t i = 0; i < vertices_added - 1; ++i)
                        {
                            draw_list::index_t src_indices[] = {index_offset + i, index_offset + i + 1};
                            std::memcpy(list.indices.data() + indices_before + indices_added, src_indices, sizeof(src_indices));
                            indices_added += 2;
                        }
                    }
                }
                break;
                default:
                    break;
            }
        }
    }

    transform_vertices(list, vertices_before, vertices_added);

    rect clip{};
    if(!list.clip_rects.empty())
    {
        clip = list.clip_rects.back();
    }

    auto blend = deduced_blend;
    if(!list.blend_modes.empty())
    {
        blend = list.blend_modes.back();
    }

    const auto add_new_command = [&](uint64_t hash) {
        list.commands.emplace_back();
        auto& command = list.commands.back();
        command.type = type;
        command.dr_type = dr_type;
        command.vertices_offset = vertices_before;
        command.indices_offset = indices_before;
        command.setup = std::forward<Setup>(setup);
        command.hash = hash;
        command.blend = blend;
        command.clip_rect = clip;
    };

    list.commands_requested++;
    if(setup.uniforms_hash == 0)
    {
        add_new_command(0);
    }
    else
    {
        auto hash = setup.uniforms_hash;
        utils::hash(hash, dr_type, type, blend, clip, setup.program.shader);
        if(!can_be_batched(list, hash))
        {
            add_new_command(hash);
        }
    }

    auto& command = list.commands.back();
    command.indices_offset = std::min(command.indices_offset, indices_before);
    command.indices_count += indices_added;
    command.vertices_offset = std::min(command.vertices_offset, vertices_before);
    command.vertices_count += vertices_added;

    return command;
}

inline void prim_resize(draw_list& list, size_t idx_count, size_t vtx_count, size_t& idx_current_idx, size_t& vtx_current_idx)
{
    idx_current_idx = list.indices.size();
    vtx_current_idx = list.vertices.size();
    list.indices.resize(idx_current_idx + idx_count);
    list.vertices.resize(vtx_current_idx + vtx_count);
}

inline void normalize2f_over_zero(float& vx, float& vy)
{
    float d2 = vx*vx + vy*vy;
    if (d2 > 0.0f)
    {
        float inv_len = 1.0f / math::sqrt(d2);
        vx *= inv_len;
        vy *= inv_len;
    }
}

inline void fixnormal2f(float& vx, float& vy)
{
    float d2 = vx*vx + vy*vy;
    float inv_lensq = 1.0f / d2;
    vx *= inv_lensq;
    vy *= inv_lensq;
}

color get_vertical_gradient(color ct,color cb, float dh, float h)
{
    const float fa = dh/h;
    const float fc = (1.0f-fa);
    return {
        uint8_t(float(ct.r) * fc + float(cb.r) * fa),
        uint8_t(float(ct.g) * fc + float(cb.g) * fa),
        uint8_t(float(ct.b) * fc + float(cb.b) * fa),
        uint8_t(float(ct.a) * fc + float(cb.a) * fa)
    };
}


const program_setup& get_simple_setup() noexcept
{
    static auto setup = []() {
        program_setup setup;
        setup.program = simple_program();
        setup.uniforms_hash = simple_hash();
        return setup;
    }();

    return setup;
}


template<typename Setup>
inline draw_cmd& add_vertices_impl(draw_list& list, draw_type dr_type, const vertex_2d* verts, size_t count,
                                   primitive_type type, const texture_view& texture, blending_mode blend, Setup&& setup)
{
    assert(verts != nullptr && count > 0 && "invalid input for memcpy");
    const auto vtx_offset = uint32_t(list.vertices.size());
    const auto idx_offset = uint32_t(list.indices.size());
    list.vertices.resize(vtx_offset + count);
    std::memcpy(list.vertices.data() + vtx_offset, verts, sizeof(vertex_2d) * count);

    if(setup.program.shader)
    {
        return add_cmd_impl(list, dr_type, vtx_offset, uint32_t(count), idx_offset, 0, type, blend, std::forward<Setup>(setup));
    }

    if(!texture)
    {
        return add_cmd_impl(list, dr_type, vtx_offset, uint32_t(count), idx_offset, 0, type, blend, get_simple_setup());
    }


    program_setup prog_setup{};

    auto has_crop = !list.crop_areas.empty();

    if(!list.programs.empty())
    {
        prog_setup.program = list.programs.back();
    }
    else
    {
        if(!has_crop)
        {
            prog_setup.program = multi_channel_texture_program();
        }
        else
        {
            blend = blending_mode::blend_normal;
            prog_setup.program = multi_channel_texture_crop_program();
        }
    }

    uint64_t hash{0};
    utils::hash(hash, texture);
    if(has_crop)
    {
        const auto& crop_areas = list.crop_areas.back();
        for(const auto& area : crop_areas)
        {
            utils::hash(hash, area);
        }
    }

    prog_setup.uniforms_hash = hash;

    auto& cmd = add_cmd_impl(list, dr_type, vtx_offset, uint32_t(count), idx_offset, 0, type, blend, std::move(prog_setup));
    // check if a new command was added
    if(!cmd.setup.begin)
    {
        if(list.crop_areas.empty())
        {
            cmd.setup.begin = [texture](const gpu_context& ctx)
            {
                ctx.program.shader->set_uniform("uTexture", texture);
            };
        }
        else
        {
            cmd.setup.begin = [texture, rects = list.crop_areas.back()](const gpu_context& ctx) mutable
            {
                if (!ctx.rend.is_with_fbo())
                {
                    for (auto& area : rects)
                    {
                        area.y = ctx.rend.get_rect().h - (area.y + area.h);
                    }
                }

                ctx.program.shader->set_uniform("uTexture", texture);
                ctx.program.shader->set_uniform("uRects[0]", rects);
                ctx.program.shader->set_uniform("uRectsCount", int(rects.size()));
            };
        }

    }

    return cmd;
}

}

math::transformf fit_item(float item_w, float item_h,
                          float area_w, float area_h,
                          size_fit sz_fit,
                          dimension_fit dim_fit)
{

    math::transformf fit_trans;
    float xscale = 1.0f;
    float yscale = 1.0f;

    switch(sz_fit)
    {
        case size_fit::shrink_to_fit:
        {
            if(item_w > area_w)
            {
                xscale = std::min(xscale, float(area_w) / item_w);
            }
            if(item_h > area_h)
            {
                yscale = std::min(yscale, float(area_h) / item_h);
            }
        }
        break;

        case size_fit::stretch_to_fit:
        {
            if(item_w < area_w)
            {
                xscale = std::max(xscale, float(area_w) / item_w);
            }
            if(item_h < area_h)
            {
                yscale = std::max(yscale, float(area_h) / item_h);
            }
        }
        break;

        case size_fit::auto_fit:
        {
            if(item_w > area_w)
            {
                xscale = std::min(xscale, float(area_w) / item_w);
            }
            else
            {
                xscale = std::max(xscale, float(area_w) / item_w);
            }

            if(item_h > area_h)
            {
                yscale = std::min(yscale, float(area_h) / item_h);
            }
            else
            {
                yscale = std::max(yscale, float(area_h) / item_h);
            }
        }
    }

    switch(dim_fit)
    {
        case dimension_fit::x:
            fit_trans.set_scale(xscale, 1.0f, 1.0f);
            break;

        case dimension_fit::y:
            fit_trans.set_scale(1.0f, yscale, 1.0f);
            break;

        case dimension_fit::uniform:
            float uniform_scale = std::min(xscale, yscale);
            fit_trans.set_scale(uniform_scale, uniform_scale, 1.0f);
            break;
    }

    return fit_trans;
}

math::transformf align_item(align_t align, const rect& item)
{
    return align_item(align, float(item.x), float(item.y), float(item.x + item.w), float(item.y + item.h), true);
}

math::transformf align_item(align_t align, float minx, float miny, float maxx, float maxy, bool pixel_snap)
{
    auto xoffs = get_alignment_x(align, minx, maxx, pixel_snap);
    auto yoffs = get_alignment_y(align, miny, miny, maxy, maxy, pixel_snap);

    math::transformf result;
    result.translate(xoffs, yoffs, 0.0f);
    return result;
}


const program_setup& empty_setup() noexcept
{
    static program_setup setup;
    return setup;
}
draw_list::draw_list()
{
    constexpr size_t vertices_reserved = 128;
    constexpr size_t indices_reserved = (vertices_reserved - 2) * 3;
    constexpr size_t commands_reserved = 64;

    cache<draw_list>::get(vertices, vertices_reserved);
    vertices.reserve(vertices_reserved);
    cache<draw_list>::get(indices, indices_reserved);
    indices.reserve(indices_reserved);
    cache<draw_list>::get(commands, commands_reserved);
    commands.reserve(commands_reserved);
}

draw_list::~draw_list()
{
    cache<draw_list>::add(vertices);
    cache<draw_list>::add(indices);
    cache<draw_list>::add(commands);
}

void draw_list::clear() noexcept
{
    vertices.clear();
    indices.clear();
    commands.clear();
    clip_rects.clear();
    blend_modes.clear();
    transforms.clear();
    programs.clear();
    commands_requested = 0;
}

void draw_list::add_rect(const std::array<math::vec2, 4>& points, color col, bool filled,
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

void draw_list::add_rect(const rect& dst, color col, bool filled, float thickness)
{
    add_rect(transform_rect(dst), col, filled, thickness);
}

void draw_list::add_rect(const frect& dst, const math::transformf& transform, color col, bool filled,
                         float thickness)
{
    add_rect(transform_rect(dst, transform), col, filled, thickness);
}

void draw_list::add_rect(const rect& dst, const math::transformf& transform, color col, bool filled,
                         float thickness)
{
    add_rect(frect(float(dst.x), float(dst.y), float(dst.w), float(dst.h)), transform, col, filled, thickness);
}

void draw_list::add_line(const math::vec2& start, const math::vec2& end, color col, float thickness)
{
    polyline line;
    line.line_to(start);
    line.line_to(end);
    add_polyline(line, col, false, thickness);
}

void draw_list::add_image(texture_view texture, const rect& src, const rect& dst,
                          const math::transformf& transform, color col, flip_format flip, const program_setup& setup)
{
    const auto rect = texture ? gfx::rect{0, 0, int(texture.width), int(texture.height)} : src;
    float left = static_cast<float>(src.x) / rect.w;
    float right = static_cast<float>(src.x + src.w) / rect.w;
    float top = static_cast<float>(src.y) / rect.h;
    float bottom = static_cast<float>(src.y + src.h) / rect.h;
    const math::vec2 min_uv = {left, top};
    const math::vec2 max_uv = {right, bottom};

    add_image(texture, transform_rect(dst, transform), col, min_uv, max_uv, flip, setup);
}

void draw_list::add_image(texture_view texture, const rect& src, const rect& dst, color col,
                          flip_format flip, const program_setup& setup)
{
    const auto rect = texture ? gfx::rect{0, 0, int(texture.width), int(texture.height)} : src;
    float left = static_cast<float>(src.x) / rect.w;
    float right = static_cast<float>(src.x + src.w) / rect.w;
    float top = static_cast<float>(src.y) / rect.h;
    float bottom = static_cast<float>(src.y + src.h) / rect.h;
    const math::vec2 min_uv = {left, top};
    const math::vec2 max_uv = {right, bottom};

    add_image(texture, dst, col, min_uv, max_uv, flip, setup);
}

void draw_list::add_image(texture_view texture, const rect& dst, color col,
                          math::vec2 min_uv, math::vec2 max_uv, flip_format flip, const program_setup& setup)
{
    add_image(texture, transform_rect(dst), col, min_uv, max_uv, flip, setup);
}

void draw_list::add_image(texture_view texture, const rect& dst, const math::transformf& transform,
                          color col, math::vec2 min_uv, math::vec2 max_uv,
                          flip_format flip, const program_setup& setup)
{
    add_image(texture, transform_rect(dst, transform), col, min_uv, max_uv, flip, setup);
}

void draw_list::add_image(texture_view texture, const point& pos, color col,
                          math::vec2 min_uv, math::vec2 max_uv, flip_format flip,
                          const program_setup& setup)
{
    const auto rect = texture ? gfx::rect{pos.x, pos.y, int(texture.width), int(texture.height)}
                              : gfx::rect{pos.x, pos.y, 0, 0};
    add_image(texture, rect, col, min_uv, max_uv, flip, setup);
}

void draw_list::add_image(texture_view texture, const std::array<math::vec2, 4>& points,
                          color col, math::vec2 min_uv, math::vec2 max_uv,
                          flip_format flip, const program_setup& setup)
{

    switch (flip)
    {
        case flip_format::horizontal:
            std::swap(min_uv.x, max_uv.x);
            break;
        case flip_format::vertical:
            std::swap(min_uv.y, max_uv.y);
            break;
        case flip_format::both:
            std::swap(min_uv.x, max_uv.x);
            std::swap(min_uv.y, max_uv.y);
            break;
        default:
        break;
    }

    const vertex_2d verts[] =
    {
        {points[0], min_uv, col},
        {points[1], {max_uv.x, min_uv.y}, col},
        {points[2], max_uv, col},
        {points[3], {min_uv.x, max_uv.y}, col},
    };

    blending_mode blend = texture.blending;

    if(col.a < 255)
    {
        blend = blending_mode::blend_normal;
    }

    add_vertices_impl(*this, draw_type::elements, verts, 4, primitive_type::triangles, texture, blend, setup);
}

void draw_list::add_vertices(draw_type dr_type, const vertex_2d* vertices, size_t count, primitive_type type, texture_view texture, const program_setup& setup)
{
    if(count == 0)
    {
        return;
    }
    add_vertices_impl(*this, dr_type, vertices, count, type, texture, texture.blending, setup);
}

void draw_list::add_list(const draw_list& list)
{
    auto vtx_offset = vertices.size();
    auto idx_offset = indices.size();
    auto cmd_offset = commands.size();

    vertices.resize(vtx_offset + list.vertices.size());
    std::memcpy(vertices.data() + vtx_offset,
                list.vertices.data(),
                list.vertices.size() * sizeof(decltype (list.vertices)::value_type));

    indices.resize(idx_offset + list.indices.size());
    std::memcpy(indices.data() + idx_offset,
                list.indices.data(),
                list.indices.size() * sizeof(decltype (list.indices)::value_type));


    if(vtx_offset != 0)
    {
        // offset the indices from the new list with the current
        for(size_t i = idx_offset, sz = indices.size(); i < sz; ++i)
        {
            indices[i] += static_cast<index_t>(vtx_offset);
        }

    }

    // These are not trivially copiable. So the memcpy is a no-no
    commands.reserve(commands.size() + list.commands.size());
    std::move(std::begin(list.commands), std::end(list.commands), std::back_inserter(commands));

    if(idx_offset != 0)
    {
        // offset the indices from the new list with the current
        for(size_t i = cmd_offset, sz = commands.size(); i < sz; ++i)
        {
            commands[i].indices_offset += static_cast<uint32_t>(idx_offset);
        }
    }

    commands_requested += list.commands_requested;
}

void draw_list::add_text(const text& t, const math::transformf& transform)
{
    if(!t.is_valid())
    {
        return;
    }

    const auto& geometry = t.get_geometry();
    if(geometry.empty())
    {
        return;
    }

    const auto& font = t.get_font();
    auto pixel_snap = font->pixel_snap;

    const auto& offsets = t.get_shadow_offsets();
    if(math::any(math::notEqual(offsets, math::vec2(0.0f, 0.0f))))
    {
        auto shadow = t;
        shadow.debug = true;
        shadow.set_vgradient_colors(t.get_shadow_color_top(), t.get_shadow_color_bot());
        shadow.set_outline_color(t.get_shadow_color_top());
        shadow.set_shadow_offsets({0.0f, 0.0f});
        math::transformf shadow_transform{};
        shadow_transform.translate(offsets.x, offsets.y, 0.0f);
        add_text(shadow, transform * shadow_transform);
    }

    float unit_length_in_pixels_at_font_position = 1.0f;
    auto sdf_spread = font->sdf_spread;
    float scale = std::max(transform.get_scale().x, transform.get_scale().y);
    float distance_field_multiplier = float(2 * sdf_spread + 1) * unit_length_in_pixels_at_font_position * scale;
    float outline_width = std::max(0.0f, t.get_outline_width());
    color outline_color = t.get_outline_color();

    // cpu_batching is disabled for text rendering with more than X vertices
    // because there are too many vertices and their matrix multiplication
    // on the cpu dominates the batching benefits.
    constexpr size_t max_cpu_transformed_glyhps = 24;
    bool cpu_batch = geometry.size() <= (max_cpu_transformed_glyhps * 4);
    bool has_multiplier = false;

    program_setup setup;
    auto texture = font->texture;

    if(sdf_spread > 0)
    {
        setup.program = distance_field_font_program();

        has_multiplier = setup.program.shader->has_uniform("uDFMultiplier");

        if(cpu_batch)
        {
            uint64_t hash{0};
            utils::hash(hash,
                        outline_width,
                        outline_color,
                        texture);

            // We have a special case here as this
            // should only be the case if that shader
            // doesn't support derivatives. Because this
            // param is affected by scale it will obstruct
            // batching, making scaled texts to not be able to
            // batch.
            if(has_multiplier)
            {
                utils::hash(hash, distance_field_multiplier);
            }

            setup.uniforms_hash = hash;
        }

    }
    else
    {
        switch(texture->get_pix_type())
        {
            case pix_type::red:
                setup.program = single_channel_texture_program();
            break;

            default:
                setup.program = multi_channel_texture_program();
            break;
        }

        if(cpu_batch)
        {
            uint64_t hash{0};
            utils::hash(hash, texture);
            setup.uniforms_hash = hash;
        }
    }

    const auto vtx_offset = uint32_t(vertices.size());
    texture_view view = texture;
    auto& cmd = add_vertices_impl(*this, draw_type::elements, geometry.data(), geometry.size(), primitive_type::triangles, view, view.blending, std::move(setup));
    if(cpu_batch)
    {
        for(size_t i = vtx_offset, sz = vertices.size(); i < sz; ++i)
        {
            auto& v = vertices[i];
            v.pos = transform.transform_coord(v.pos);

            if(pixel_snap)
            {
                v.pos.x = float(int(v.pos.x));
            }
        }
    }

    // check if a new command was added
    if(!cmd.setup.begin)
    {
        if(sdf_spread > 0)
        {
            cmd.setup.begin = [transform = transform,
                               distance_field_multiplier,
                               outline_width,
                               outline_color,
                               texture,
                               cpu_batch,
                               pixel_snap,
                               has_multiplier](const gpu_context& ctx) mutable
            {
                if(!cpu_batch)
                {
                    if(pixel_snap)
                    {
                        auto pos = transform.get_position();
                        transform.set_position(float(int(pos.x)), pos.y, pos.z);
                    }

                    ctx.rend.push_transform(transform);
                }
                if(has_multiplier)
                {
                    ctx.program.shader->set_uniform("uDFMultiplier", distance_field_multiplier);
                }
                ctx.program.shader->set_uniform("uOutlineWidth", outline_width);
                ctx.program.shader->set_uniform("uOutlineColor", outline_color);
                ctx.program.shader->set_uniform("uTexture", texture);
            };
        }
        else
        {
            cmd.setup.begin = [transform = transform, texture, cpu_batch, pixel_snap](const gpu_context& ctx) mutable
            {
                if(!cpu_batch)
                {
                    if(pixel_snap)
                    {
                        auto pos = transform.get_position();
                        transform.set_position(float(int(pos.x)), pos.y, pos.z);
                    }
                    ctx.rend.push_transform(transform);
                }
                ctx.program.shader->set_uniform("uTexture", texture);
            };
        }

    }

    if(!cmd.setup.end && !cpu_batch)
    {
        cmd.setup.end = [](const gpu_context& ctx)
        {
            ctx.rend.pop_transform();
        };
    }

    if(debug_draw)
    {
        add_text_debug_info(t, transform);
    }
}

void draw_list::add_text(const text& t, const math::transformf& transform, const rect& dst_rect, size_fit sz_fit,
                         dimension_fit dim_fit)
{
    auto align = t.get_alignment();
    auto scale_x = transform.get_scale().x;
    auto scale_y = transform.get_scale().y;
    float text_width = t.get_width() * scale_x;
    float text_height = t.get_height() * scale_y;

    auto offsets = text::get_alignment_offsets(align,
                                               float(dst_rect.x),
                                               float(dst_rect.y),
                                               float(dst_rect.y),
                                               float(dst_rect.x + dst_rect.w),
                                               float(dst_rect.y + dst_rect.h),
                                               float(dst_rect.y + dst_rect.h),
                                               false);

    math::transformf parent{};
    parent.translate(transform.get_position());
    parent.set_rotation(transform.get_rotation());

    math::transformf local = fit_item(text_width,
                                      text_height,
                                      float(dst_rect.w),
                                      float(dst_rect.h),
                                      sz_fit, dim_fit);

    local.scale(transform.get_scale());
    local.translate(-math::vec3(offsets.first, offsets.second, 0.0f));

    add_text(t, parent * local);
}


void draw_list::add_text_superscript(const text& whole_text, const text& script_text,
                                     const math::transformf& transform,
                                     float script_scale)
{
    add_text_superscript_impl(whole_text, script_text, transform, script_scale);
}

void draw_list::add_text_superscript(const text& whole_text, const text& script_text,
                                     const math::transformf& transform, const rect& dst_rect,
                                     float script_scale, size_fit sz_fit,
                                     dimension_fit dim_fit)
{
    if(!whole_text.is_valid() && !script_text.is_valid())
    {
        return;
    }
    if(whole_text.is_valid() && !script_text.is_valid())
    {
        assert(whole_text.get_alignment() == script_text.get_alignment() && "Both texts should have the same alignment.");
    }
    auto align = [&]()
    {
        if(whole_text.is_valid())
        {
            return whole_text.get_alignment();
        }

        return script_text.get_alignment();
    }();

    auto scale_whole = transform.get_scale();
    auto scale_partial = script_scale * transform.get_scale();
    float text_width = whole_text.get_width() * scale_whole.x + script_text.get_width() * scale_partial.x;
    float text_height = whole_text.get_height() * scale_whole.y;

    auto offsets = text::get_alignment_offsets(align,
                                               float(dst_rect.x),
                                               float(dst_rect.y),
                                               float(dst_rect.y),
                                               float(dst_rect.x + dst_rect.w),
                                               float(dst_rect.y + dst_rect.h),
                                               float(dst_rect.y + dst_rect.h),
                                               false);

    math::transformf parent{};
    parent.translate(transform.get_position());
    parent.set_rotation(transform.get_rotation());

    math::transformf local = fit_item(text_width,
                                      text_height,
                                      float(dst_rect.w),
                                      float(dst_rect.h),
                                      sz_fit, dim_fit);
    local.scale(transform.get_scale());
    local.translate(-math::vec3(offsets.first, offsets.second, 0.0f));

    add_text_superscript_impl(whole_text, script_text, parent * local, script_scale);
}

void draw_list::add_text_superscript_impl(const text& whole_text, const text& script_text,
                                          const math::transformf& transform,
                                          float script_scale)
{
    if(!whole_text.is_valid() && !script_text.is_valid())
    {
        return;
    }
    if(whole_text.is_valid() && !script_text.is_valid())
    {
        assert(whole_text.get_alignment() == script_text.get_alignment() && "Both texts should have the same alignment.");
    }
    auto align = [&]()
    {
        if(whole_text.is_valid())
        {
            return whole_text.get_alignment();
        }

        return script_text.get_alignment();
    }();

    if(whole_text.get_lines().size() > 1)
    {
        log("Superscript text should not be multiline. This api will not behave properly.");
    }
    const auto whole_text_width = whole_text.get_width();

    const auto whole_text_min_baseline_height = whole_text.get_utf8_text().empty() ?
                                                script_text.get_min_baseline_height() :
                                                whole_text.get_min_baseline_height();
    const auto whole_text_max_baseline_height = whole_text.get_utf8_text().empty() ?
                                                script_text.get_max_baseline_height() :
                                                whole_text.get_max_baseline_height();

    const auto script_text_width = script_text.get_width() * script_scale;
    const auto text_partial_height = whole_text.get_utf8_text().empty() ?
                                     script_text.get_height() * (1.0f - script_scale) :
                                     whole_text.get_height() - (script_text.get_height() * script_scale);

    const auto script_text_min_baseline_height =
        script_text.get_min_baseline_height() * script_scale;
    const auto script_text_max_baseline_height =
        script_text.get_max_baseline_height() * script_scale;

    const auto half_whole_text_width = whole_text_width * 0.5f;
    const auto half_script_text_width = script_text_width * 0.5f;
    const auto half_text_partial_height = text_partial_height * 0.5f;

    math::transformf whole_transform{};
    math::transformf script_transform{};
    script_transform.scale({script_scale, script_scale, 1.0f});


    if(align & align::top)
    {
    }

    if(align & align::middle)
    {
        script_transform.translate(0.0f, -half_text_partial_height, 0.0f);
    }

    if(align & align::bottom)
    {
        script_transform.translate(0.0f, -text_partial_height, 0.0f);
    }

    if(align & align::baseline_top)
    {
        script_transform.translate(0.0f, script_text_min_baseline_height - whole_text_min_baseline_height, 0.0f);
    }

    if(align & align::baseline_bottom)
    {

        script_transform.translate(0.0f, script_text_max_baseline_height - whole_text_max_baseline_height, 0.0f);
    }


    if(align & align::left)
    {
        script_transform.translate(whole_text_width, 0.0f, 0.0f);
    }

    if(align & align::right)
    {
        whole_transform.translate(-script_text_width, 0.0f, 0.0f);
    }

    if(align & align::center)
    {
        whole_transform.translate(-half_script_text_width, 0.0f, 0.0f);
        script_transform.translate(half_whole_text_width, 0.0f, 0.0f);
    }

    add_text(whole_text, transform * whole_transform);
    add_text(script_text, transform * script_transform);
}

void draw_list::add_text_subscript(const text& whole_text,
                                   const text& script_text,
                                   const math::transformf& transform,
                                   float script_scale)
{
    add_text_subscript_impl(whole_text, script_text, transform, script_scale);
}

void draw_list::add_text_subscript(const text& whole_text,
                                   const text& script_text,
                                   const math::transformf& transform,
                                   const rect& dst_rect,
                                   float script_scale,
                                   size_fit sz_fit,
                                   dimension_fit dim_fit)
{
    if(!whole_text.is_valid() && !script_text.is_valid())
    {
        return;
    }
    if(whole_text.is_valid() && !script_text.is_valid())
    {
        assert(whole_text.get_alignment() == script_text.get_alignment() && "Both texts should have the same alignment.");
    }
    auto align = [&]()
    {
        if(whole_text.is_valid())
        {
            return whole_text.get_alignment();
        }

        return script_text.get_alignment();
    }();

    auto scale_whole = transform.get_scale();
    auto scale_partial = script_scale * transform.get_scale();
    float text_width = whole_text.get_width() * scale_whole.x + script_text.get_width() * scale_partial.x;
    float text_height = whole_text.get_height() * scale_whole.y;

    auto offsets = text::get_alignment_offsets(align,
                                               float(dst_rect.x),
                                               float(dst_rect.y),
                                               float(dst_rect.y),
                                               float(dst_rect.x + dst_rect.w),
                                               float(dst_rect.y + dst_rect.h),
                                               float(dst_rect.y + dst_rect.h),
                                               false);

    math::transformf parent{};
    parent.translate(transform.get_position());
    parent.set_rotation(transform.get_rotation());

    math::transformf local = fit_item(text_width,
                                      text_height,
                                      float(dst_rect.w),
                                      float(dst_rect.h),
                                      sz_fit, dim_fit);

    local.scale(transform.get_scale());
    local.translate(-math::vec3(offsets.first, offsets.second, 0.0f));

    add_text_subscript_impl(whole_text, script_text, parent * local, script_scale);
}

void draw_list::add_text_subscript_impl(const text& whole_text,
                                        const text& script_text,
                                        const math::transformf& transform,
                                        float script_scale)
{
    if(!whole_text.is_valid() && !script_text.is_valid())
    {
        return;
    }
    if(whole_text.is_valid() && !script_text.is_valid())
    {
        assert(whole_text.get_alignment() == script_text.get_alignment() && "Both texts should have the same alignment.");
    }
    auto align = [&]()
    {
        if(whole_text.is_valid())
        {
            return whole_text.get_alignment();
        }

        return script_text.get_alignment();
    }();
    if(whole_text.get_lines().size() > 1)
    {
        log("Subscript text should not be multiline. This api will not behave properly.");
    }

    const auto whole_text_width = whole_text.get_width();

    const auto whole_text_height = whole_text.get_utf8_text().empty() ?
                                   script_text.get_height() :
                                   whole_text.get_height();

    const auto whole_text_max_baseline_height = whole_text.get_utf8_text().empty() ?
                                                script_text.get_max_baseline_height() :
                                                whole_text.get_max_baseline_height();

    const auto script_text_width = script_text.get_width() * script_scale;
    const auto text_partial_height = script_scale * script_text.get_height();
    const auto text_partial_height_dif = whole_text_height - text_partial_height;

    const auto script_text_max_baseline_height =
        script_text.get_max_baseline_height() * script_scale;

    const auto half_whole_text_width = whole_text_width * 0.5f;
    const auto half_script_text_width = script_text_width * 0.5f;
    const auto half_text_partial_height_dif = text_partial_height_dif * 0.5f;

    const auto top_y_aligns_partial_offset = whole_text_max_baseline_height - script_text_max_baseline_height;
    const auto center_y_aligns_partial_offset = whole_text_max_baseline_height - (half_text_partial_height_dif + script_text_max_baseline_height);
    const auto bottom_y_aligns_partial_offset = - ((whole_text_height - whole_text_max_baseline_height) -
                                                  (text_partial_height - script_text_max_baseline_height));

    math::transformf whole_transform{};
    math::transformf script_transform{};
    script_transform.scale({script_scale, script_scale, 1.0f});


    if(align & align::top)
    {
        script_transform.translate(0.0f, top_y_aligns_partial_offset, 0.0f);
    }

    if(align & align::middle)
    {
        script_transform.translate(0.0f, center_y_aligns_partial_offset, 0.0f);
    }

    if(align & align::bottom)
    {
        script_transform.translate(0.0f, bottom_y_aligns_partial_offset, 0.0f);
    }

    if(align & align::baseline_top)
    {
    }

    if(align & align::baseline_bottom)
    {
    }


    if(align & align::left)
    {
        script_transform.translate(whole_text_width, 0.0f, 0.0f);
    }

    if(align & align::right)
    {
        whole_transform.translate(-script_text_width, 0.0f, 0.0f);
    }

    if(align & align::center)
    {
        whole_transform.translate(-half_script_text_width, 0.0f, 0.0f);
        script_transform.translate(half_whole_text_width, 0.0f, 0.0f);
    }

    add_text(whole_text, transform * whole_transform);
    add_text(script_text, transform * script_transform);
}

void draw_list::set_debug_draw(bool debug)
{
    debug_draw = debug;
}

void draw_list::toggle_debug_draw()
{
    debug_draw = !debug_draw;
}

std::string draw_list::to_string() const
{
    std::stringstream ss;
    ss << "\n";
    ss << "[REQUESTED CALLS]: " << commands_requested;
    ss << "\n";
    ss << "[RENDERED CALLS]: " << commands.size();
    ss << "\n";
    ss << "[BATCHED CALLS]: " << commands_requested - commands.size();
    ss << "\n";
    ss << "[VERTICES]: " << vertices.size();
    ss << "\n";
    ss << "[INDICES]: " << indices.size();
    return ss.str();
}

void draw_list::validate_stacks() const noexcept
{
    assert(clip_rects.empty() && "draw_list::clip_rects stack was not popped");
    assert(crop_areas.empty() && "draw_list::crop_areas stack was not popped");
    assert(blend_modes.empty() && "draw_list::blend_modes stack was not popped");
    assert(transforms.empty() && "draw_list::transforms stack was not popped");
    assert(programs.empty() && "draw_list::programs stack was not popped");

}

void draw_list::add_polyline(const polyline& poly, color col, bool closed, float thickness, float antialias_size)
{
    add_polyline_gradient(poly, col, col, closed, thickness, antialias_size);
}


void draw_list::add_polyline_gradient(const polyline& poly, color coltop, color colbot, bool closed, float thickness, float antialias_size)
{
    blending_mode blend = (coltop.a < 255 || colbot.a < 255 || antialias_size != 0.0f) ? blending_mode::blend_normal : blending_mode::blend_none;

    const auto& points = poly.get_points();
    auto points_count = points.size();

    if (points_count < 2)
        return;

    size_t count = points_count;
    if (!closed)
        count = points_count-1;

    auto vtx_offset = vertices.size();
    auto idx_offset = indices.size();

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
        prim_resize(*this, idx_count, vtx_count, idx_current_idx, vtx_current_idx);

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
        prim_resize(*this, idx_count, vtx_count, idx_current_idx, vtx_current_idx);

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

    auto vtx_count = vertices.size() - vtx_offset;
    auto idx_count = indices.size() - idx_offset;
    add_cmd_impl(*this,
                 draw_type::elements,
                 uint32_t(vtx_offset),
                 uint32_t(vtx_count),
                 uint32_t(idx_offset),
                 uint32_t(idx_count),
                 primitive_type::triangles,
                 blend,
                 get_simple_setup());
}


void draw_list::add_polyline_filled_convex(const polyline& poly, color col, float antialias_size)
{
    add_polyline_filled_convex_gradient(poly, col, col, antialias_size);
}

void draw_list::add_polyline_filled_convex_gradient(const polyline& poly, color coltop, color colbot, float antialias_size)
{
    blending_mode blend = (coltop.a < 255 || colbot.a < 255 || antialias_size != 0.0f) ? blending_mode::blend_normal : blending_mode::blend_none;

    const auto& points = poly.get_points();
    auto points_count = points.size();

    if (points_count < 3)
        return;

    auto vtx_offset = vertices.size();
    auto idx_offset = indices.size();

    float miny = 999999999.0f;
    float maxy = -miny;

    float minx = 999999999.0f;
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
        prim_resize(*this, idx_count, vtx_count, idx_current_idx, vtx_current_idx);

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
        prim_resize(*this, idx_count, vtx_count, idx_current_idx, vtx_current_idx);

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

    auto vtx_count = vertices.size() - vtx_offset;
    auto idx_count = indices.size() - idx_offset;
    add_cmd_impl(*this,
                 draw_type::elements,
                 uint32_t(vtx_offset),
                 uint32_t(vtx_count),
                 uint32_t(idx_offset),
                 uint32_t(idx_count),
                 primitive_type::triangles,
                 blend,
                 get_simple_setup());

}
void draw_list::add_ellipse(const math::vec2& center, const math::vec2& radii, color col, size_t num_segments, float thickness)
{
    add_ellipse_gradient(center, radii, col, col, num_segments, thickness);

}

void draw_list::add_ellipse_gradient(const math::vec2& center, const math::vec2& radii, color col1, color col2, size_t num_segments, float thickness)
{
    if ((col1.a == 0 && col2.a == 0) || num_segments <= 2)
        return;

    polyline line;
    line.ellipse(center, radii, num_segments);
    add_polyline_gradient(line, col1, col2, true, thickness);

}

void draw_list::add_ellipse_filled(const math::vec2& center, const math::vec2& radii, color col, size_t num_segments)
{
    if (col.a == 0|| num_segments <= 2)
        return;

    polyline line;
    line.ellipse(center, radii, num_segments);
    add_polyline_filled_convex(line, col, true);
}

void draw_list::add_bezier_curve(const math::vec2& pos0, const math::vec2& cp0, const math::vec2& cp1, const math::vec2& pos1, color col, float thickness, int num_segments)
{
    if (col.a == 0)
        return;

    polyline line;
    line.line_to(pos0);
    line.bezier_curve_to(cp0, cp1, pos1, num_segments);
    add_polyline(line, col, false, thickness);
}

void draw_list::add_curved_path_gradient(const std::vector<math::vec2>& points, color col1, color col2, float thickness, float antialias_size)
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

void draw_list::push_crop(const crop_area_t& crop)
{
    crop_areas.emplace_back(crop);
}

void draw_list::pop_crop()
{
    if(!crop_areas.empty())
    {
        crop_areas.pop_back();
    }
}

void draw_list::push_blend(blending_mode blend)
{
    blend_modes.emplace_back(blend);
}

void draw_list::pop_blend()
{
    if(!blend_modes.empty())
    {
        blend_modes.pop_back();
    }
}
void draw_list::push_transform(const math::transformf& tr)
{
    transforms.emplace_back(tr);
}

void draw_list::pop_transform()
{
    if(!transforms.empty())
    {
        transforms.pop_back();
    }
}

void draw_list::push_program(const gpu_program& program)
{
    programs.emplace_back(program);
}

void draw_list::pop_program()
{
    if(!programs.empty())
    {
        programs.pop_back();
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
    if(t.debug)
    {
        return;
    }

    const auto& lines = t.get_lines_metrics();

//    math::transformf tr = transform;
//    {
//        auto col = color::cyan();
//        std::string desc = "ascent ";
//        for(const auto& line : lines)
//        {
//            auto v1 = transform.transform_coord({line.minx, line.ascent});
//            //auto v2 = transform.transform_coord({line.maxx, line.ascent});

//            text txt;
//            txt.debug = true;
//            txt.set_color(col);
//            txt.set_font(default_font());
//            txt.set_alignment(align::right | align::middle);
//            txt.set_utf8_text(desc);

//            tr.set_position(v1.x, v1.y, 0.0f);
//            add_text(txt, tr);

////            auto width = line.maxx - line.minx;
////            std::stringstream ss;
////            ss << " width = " << std::fixed << std::setprecision(2) << width;

////            txt.set_alignment(text::alignment::left | align::middle);
////            txt.set_utf8_text(ss.str());

////            tr.set_position(v2.x, v1.y, 0.0f);
////            add_text(txt, tr);
//        }
//    }
//    {
//        auto col = color::red();
//        std::string desc = "median ";
//        for(const auto& line : lines)
//        {
//            auto v1 = transform.transform_coord({line.minx, line.median});

//            text txt;
//            txt.debug = true;
//            txt.set_color(col);
//            txt.set_font(default_font());
//            txt.set_alignment(align::right | align::middle);
//            txt.set_utf8_text(desc);

//            tr.set_position(v1.x, v1.y, 0.0f);
//            add_text(txt, tr);
//        }
//    }
//    {
//        auto col = color::magenta();
//        std::string desc = "baseline ";
//        for(const auto& line : lines)
//        {
//            auto v1 = transform.transform_coord({line.minx, line.baseline});

//            text txt;
//            txt.debug = true;
//            txt.set_color(col);
//            txt.set_font(default_font());
//            txt.set_alignment(align::right | align::middle);
//            txt.set_utf8_text(desc);

//            tr.set_position(v1.x, v1.y, 0.0f);
//            add_text(txt, tr);
//        }
//    }
//    {
//        auto col = color::blue();
//        std::string desc = "descent ";
//        for(const auto& line : lines)
//        {
//            auto v1 = transform.transform_coord({line.minx, line.descent});

//            text txt;
//            txt.debug = true;
//            txt.set_color(col);
//            txt.set_font(default_font());
//            txt.set_alignment(align::right | align::middle);
//            txt.set_utf8_text(desc);

//            tr.set_position(v1.x, v1.y, 0.0f);
//            add_text(txt, tr);
//        }
//    }


    {
        auto geometry = t.get_geometry();
        for(auto& v : geometry)
        {
            v.pos = transform.transform_coord(v.pos);
        }

        for(size_t i = 0, sz = geometry.size(); i < sz; i+=4)
        {
            polyline line;
            color col = geometry[i].col;
            for(size_t j = 0; j < 4; ++j)
            {
                line.line_to(geometry[i + j].pos);
            }
            add_polyline(line, col, true);
        }
    }

    const auto& line_path = t.get_line_path();
    const auto& rect = t.get_frect();
    push_transform(transform);

    if(!line_path.empty())
    {
        add_polyline(line_path, color::yellow(), false);
    }
    else
    {
        add_rect(rect, {}, color::red(), false, 1.0f);

        {
            auto col = color::cyan();
            for(const auto& line : lines)
            {
                math::vec2 v1{line.minx, line.ascent};
                math::vec2 v2{line.maxx, line.ascent};

                add_line(v1, v2, col);
            }
        }
        {
            auto col = color::magenta();
            for(const auto& line : lines)
            {
                math::vec2 v1{line.minx, line.baseline};
                math::vec2 v2{line.maxx, line.baseline};

                add_line(v1, v2, col);
            }
        }
        {
            auto col = color::red();
            for(const auto& line : lines)
            {
                math::vec2 v1{line.minx, line.median};
                math::vec2 v2{line.maxx, line.median};

                add_line(v1, v2, col);
            }
        }
        {
            auto col = color::blue();
            for(const auto& line : lines)
            {
                math::vec2 v1{line.minx, line.descent};
                math::vec2 v2{line.maxx, line.descent};

                add_line(v1, v2, col, 1.0f);
            }
        }

        {
            auto col = color::green();
            for(const auto& line : lines)
            {
                auto line_height = t.get_font()->line_height;
                math::vec2 v1{line.maxx, line.ascent};
                math::vec2 v2{line.maxx, line.ascent + line_height};

                add_line(v1, v2, col);

            }
        }
    }

    pop_transform();
}


}
