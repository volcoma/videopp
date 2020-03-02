#include "draw_list.h"
#include <cmath>
#include <codecvt>
#include <locale>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "font.h"
#include "logger.h"
#include "renderer.h"
#include "text.h"
#include "draw_cmd.h"

namespace gfx
{

namespace
{
bool debug_draw_enabled = false;


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

inline void transform_vertices(draw_list& list, size_t vtx_offset, size_t vtx_count, bool pixel_snap)
{
    if(!list.transforms.empty())
    {
        const auto& tr = list.transforms.back();
        for(size_t i = vtx_offset; i < vtx_offset + vtx_count; ++i)
        {
            auto& v = list.vertices[i];
            v.pos = tr.transform_coord(v.pos);

            if(pixel_snap)
            {
                v.pos.x = float(int(v.pos.x));
            }
        }
    }
}

template<typename Setup>
inline draw_cmd& add_cmd_impl(draw_list& list, draw_type dr_type, uint32_t vertices_before, uint32_t vertices_added,
                              uint32_t indices_before, uint32_t indices_added, primitive_type type, blending_mode deduced_blend, Setup&& setup,
                              bool apply_transform = true, bool pixel_snap = false)
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
                            draw_list::index_t src_indices[] =
                            {
                                index_offset,
                                index_offset + i - 1,
                                index_offset + i
                            };
                            std::memcpy(list.indices.data() + indices_before + indices_added,
                                        src_indices, sizeof(src_indices));
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
                            draw_list::index_t src_indices[] =
                            {
                                index_offset + i,
                                index_offset + i + 1
                            };
                            std::memcpy(list.indices.data() + indices_before + indices_added,
                                        src_indices, sizeof(src_indices));
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

    if(apply_transform)
    {
        transform_vertices(list, vertices_before, vertices_added, pixel_snap);
    }

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
        setup.program = get_program<programs::simple>();
        setup.uniforms_hash = simple_hash();
        return setup;
    }();

    return setup;
}


program_setup::callback transform_setup(const draw_list& list, bool cpu_batch, bool pixel_snap)
{
    if(!cpu_batch)
    {
        return [transform = list.transforms.back(), pixel_snap](const gpu_context& ctx) mutable
        {
            auto pos = transform.get_position();

            if(pixel_snap)
            {
                pos.x = float(int(pos.x));
            }
            transform.set_position(pos.x, pos.y, 0);

            //push the transformation to happen on the gpu
            ctx.rend.push_transform(transform);
        };
    }

    return nullptr;
}

program_setup::callback crop_rects_setup(const draw_list& list)
{
    if(!list.crop_areas.empty())
    {
        return [rects = list.crop_areas.back()](const gpu_context& ctx) mutable
        {
            if (!ctx.rend.is_with_fbo())
            {
                for (auto& area : rects)
                {
                    area.y = ctx.rend.get_rect().h - (area.y + area.h);
                }
            }
            ctx.program.shader->set_uniform("uRects[0]", rects);
            ctx.program.shader->set_uniform("uRectsCount", int(rects.size()));
        };
    }

    return nullptr;
}

template<typename Setup>
inline draw_cmd& add_vertices_impl(draw_list& list, draw_type dr_type, const vertex_2d* verts, size_t count,
                                   primitive_type type, const texture_view& texture, blending_mode blend, Setup&& setup, bool apply_transform = true, bool pixel_snap = false)
{
    assert(verts != nullptr && count > 0 && "invalid input for memcpy");
    const auto vtx_offset = uint32_t(list.vertices.size());
    const auto idx_offset = uint32_t(list.indices.size());
    list.vertices.resize(vtx_offset + count);
    std::memcpy(list.vertices.data() + vtx_offset, verts, sizeof(vertex_2d) * count);

    if(setup.program.shader)
    {
        return add_cmd_impl(list, dr_type, vtx_offset, uint32_t(count), idx_offset, 0, type, blend, std::forward<Setup>(setup), apply_transform, pixel_snap);
    }

    if(!texture)
    {
        return add_cmd_impl(list, dr_type, vtx_offset, uint32_t(count), idx_offset, 0, type, blend, get_simple_setup(), apply_transform, pixel_snap);
    }


    program_setup prog_setup{};

    auto has_crop = !list.crop_areas.empty();

    if(!list.programs.empty())
    {
        prog_setup.program = list.programs.back();
    }
    else
    {
        switch(texture.format)
        {
        case pix_type::red:
            if(!has_crop)
            {
                prog_setup.program = get_program<programs::single_channel>();
            }
            else
            {
                blend = blending_mode::blend_normal;
                prog_setup.program = get_program<programs::single_channel_crop>();
            }
            break;

        default:
            if(!has_crop)
            {
                prog_setup.program = get_program<programs::multi_channel>();
            }
            else
            {
                blend = blending_mode::blend_normal;
                prog_setup.program = get_program<programs::multi_channel_crop>();
            }
            break;
        }
    }

    if(apply_transform)
    {
        utils::hash(prog_setup.uniforms_hash, texture);
        if(has_crop)
        {
            const auto& crop_areas = list.crop_areas.back();
            for(const auto& area : crop_areas)
            {
                utils::hash(prog_setup.uniforms_hash, area);
            }
        }
    }

    auto& cmd = add_cmd_impl(list, dr_type, vtx_offset, uint32_t(count), idx_offset, 0, type, blend, std::move(prog_setup), apply_transform, pixel_snap);
    // check if a new command was added
    if(!cmd.setup.begin)
    {
        cmd.setup.begin = [setup_crop_rects = crop_rects_setup(list),
                           setup_transform = transform_setup(list, apply_transform, pixel_snap),
                           texture](const gpu_context& ctx) mutable
        {
            if(setup_transform)
            {
                setup_transform(ctx);
            }

            if(setup_crop_rects)
            {
                setup_crop_rects(ctx);
            }

            ctx.program.shader->set_uniform("uTexture", texture);

        };
    }

    if(!cmd.setup.end && !apply_transform)
    {
        cmd.setup.end = [](const gpu_context& ctx)
        {
            ctx.rend.pop_transform();
        };
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

math::transformf align_and_fit_item(align_t align, float item_w, float item_h, const math::transformf &transform,
                                         const rect& dst_rect, size_fit sz_fit, dimension_fit dim_fit)
{
    auto scale_x = transform.get_scale().x;
	auto scale_y = transform.get_scale().y;
	float text_width = item_w * scale_x;
	float text_height = item_h * scale_y;

	const auto& translation = transform.get_position();
	auto offsets = get_alignment_offsets(align,
										 float(dst_rect.x),
										 float(dst_rect.y),
										 float(dst_rect.x + dst_rect.w),
										 float(dst_rect.y + dst_rect.h),
                                         false);

    math::transformf parent{};
    parent.translate(translation);
    parent.set_rotation(transform.get_rotation());

	math::transformf local = fit_item(text_width,
									  text_height,
									  float(dst_rect.w),
									  float(dst_rect.h),
                                      sz_fit, dim_fit);

    local.scale(transform.get_scale());
    local.translate(-math::vec3(offsets.first, offsets.second, 0.0f));

    return parent * local;
}

math::transformf align_and_fit_text(const text& t, const math::transformf &transform,
									const rect& dst_rect, size_fit sz_fit, dimension_fit dim_fit)
{
	return align_and_fit_item(t.get_alignment(), t.get_width(), t.get_height(), transform, dst_rect, sz_fit, dim_fit);
}

math::transformf align_item(align_t align, const rect& item)
{
    return align_item(align, float(item.x), float(item.y), float(item.x + item.w), float(item.y + item.h), true);
}

math::transformf align_item(align_t align, float minx, float miny, float maxx, float maxy, bool pixel_snap)
{
    auto xoffs = get_alignment_x(align, minx, maxx, pixel_snap);
	auto yoffs = get_alignment_y(align, miny, maxy, pixel_snap);

    math::transformf result;
    result.translate(xoffs, yoffs, 0.0f);
    return result;
}


math::transformf align_wrap_and_fit_text(text& t,
										const math::transformf& transform,
                                        rect dst_rect,
                                        size_fit sz_fit,
                                        dimension_fit dim_fit)
{
    using clock_t = std::chrono::steady_clock;
	auto start = clock_t::now();

	auto max_w = dst_rect.w;
	t.set_wrap_width(float(max_w));
	auto world = align_and_fit_text(t, transform, dst_rect, sz_fit, dim_fit);
	auto w = int(float(dst_rect.w) / world.get_scale().y);

    size_t iterations{0};
	if(w != max_w)
	{
		max_w = w;

        auto fit = dim_fit;
        dim_fit = dimension_fit::y;
		while(iterations < 128)
		{
			t.set_wrap_width(float(max_w));
			world = align_and_fit_text(t, transform, dst_rect, sz_fit, dim_fit);
            w = int(float(dst_rect.w) / world.get_scale().y);

            auto diff = w - max_w;

            if(diff >= 0)
			{
                if(fit != dim_fit)
                {
                    //we hit the upper bounds, so return to
                    //the preferred method
                    dim_fit = fit;
                    w -= diff / 2;
                }
                else
                {
                    break;
                }
			}

            max_w = w;
			iterations++;
		}
	}

	auto end = clock_t::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	std::cout << "wrap fitting : i=" << iterations << ", us=" << dur.count() << std::endl;
	return world;
}


math::transformf align_wrap_and_fit_text(rich_text& t, const math::transformf& transform, rect dst_rect, size_fit sz_fit, dimension_fit dim_fit)
{
	auto modified_transform = transform;

	const auto& style = t.get_style();
	auto font = style.font;
	auto line_height = font ? font->line_height : 0.0f;

	auto advance = (t.get_calculated_line_height() - line_height);
	modified_transform.translate(0.0f, advance * 0.5f, 0.0f);
	dst_rect.h -= int(advance);

	return align_wrap_and_fit_text(static_cast<text&>(t), modified_transform, dst_rect, sz_fit, dim_fit);
}


const program_setup& empty_setup() noexcept
{
    static program_setup setup;
    return setup;
}
draw_list::draw_list(bool has_debug)
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

    if(has_debug)
    {
        debug = std::make_unique<draw_list>(false);
    }
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

    if(debug)
    {
        debug->clear();
    }
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

    if(!texture)
    {
        col = color::magenta();
        col.a = 128;
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

	if(debug_draw() && debug)
    {
        debug->add_rect(points, col, false);
    }
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

    if(debug && list.debug)
    {
        debug->add_list(*list.debug);
    }
}

void draw_list::add_text(const text& t, const math::transformf& transform)
{
    if(!t.is_valid())
    {
        return;
    }

	const auto& style = t.get_style();
	auto font = style.font;
	auto pixel_snap = font->pixel_snap;

	const auto& offsets = style.shadow_offsets * style.scale;
	if(math::any(math::notEqual(offsets, math::vec2(0.0f, 0.0f))))
	{
		auto shadow = t;
		shadow.set_vgradient_colors(style.shadow_color_top, style.shadow_color_bot);
		shadow.set_outline_vgradient_colors(style.shadow_color_top, style.shadow_color_bot);
		shadow.set_shadow_offsets({0.0f, 0.0f});
		math::transformf shadow_transform{};
		shadow_transform.translate(offsets.x, offsets.y, 0.0f);

		auto debug_draw_old = set_debug_draw(false);
		add_text(shadow, transform * shadow_transform);
		set_debug_draw(debug_draw_old);

    }

	const auto& geometry = t.get_geometry();
	if(!geometry.empty())
	{
        auto sdf_spread = font->sdf_spread;

        // cpu_batching is disabled for text rendering with more than X vertices
        // because there are too many vertices and their matrix multiplication
        // on the cpu dominates the batching benefits.
        constexpr size_t max_cpu_transformed_glyhps = 24;
        auto has_crop = !crop_areas.empty();
        bool cpu_batch = geometry.size() <= (max_cpu_transformed_glyhps * 4);

        program_setup setup;
        auto texture = font->texture;
        texture_view view = texture;
        auto blend = view.blending;

        if(sdf_spread > 0)
        {
            if(!has_crop)
            {
                setup.program = get_program<programs::distance_field>();
            }
            else
            {
                setup.program = get_program<programs::distance_field_crop>();
            }


            if(cpu_batch)
            {
                utils::hash(setup.uniforms_hash,
                            texture);

                if(has_crop)
                {
                    const auto& areas = crop_areas.back();
                    for(const auto& area : areas)
                    {
                        utils::hash(setup.uniforms_hash, area);
                    }
                }
            }
        }


        push_transform(transform);
        auto& cmd = add_vertices_impl(*this,
                                      draw_type::elements,
                                      geometry.data(), geometry.size(),
                                      primitive_type::triangles,
                                      view, blend,
                                      std::move(setup),
                                      cpu_batch, pixel_snap);


        // check if a new command was added
        if(!cmd.setup.begin)
        {

            if(sdf_spread > 0)
            {
                cmd.setup.begin = [setup_crop_rects = crop_rects_setup(*this),
                                   setup_transform = transform_setup(*this, cpu_batch, pixel_snap),
                                   texture](const gpu_context& ctx) mutable
                {
                    if(setup_transform)
                    {
                        setup_transform(ctx);
                    }

                    if(setup_crop_rects)
                    {
                        setup_crop_rects(ctx);
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

        pop_transform();
    }
	if(debug_draw() && debug)
    {
        debug->add_text_debug_info(t, transform);
    }
}

void draw_list::add_text(const text& t, const math::transformf& transform, const rect& dst_rect, size_fit sz_fit,
                         dimension_fit dim_fit)
{
	add_text(t, align_and_fit_text(t, transform, dst_rect, sz_fit, dim_fit));

	if(debug_draw() && debug)
    {
        auto tr = transform;
        tr.set_scale(1.0f, 1.0f, 1.0f);
        debug->add_rect(dst_rect, tr, color::cyan(), false);
    }
}

void draw_list::add_text(const rich_text& t, const math::transformf& transform)
{
	add_text(static_cast<const text&>(t), transform);

	auto sorted_texts = t.get_embedded_texts();
	std::sort(std::begin(sorted_texts), std::end(sorted_texts), [](const auto& lhs, const auto& rhs)
	{
		return lhs->text.get_style().font < rhs->text.get_style().font;
	});

	for(const auto& ptr : sorted_texts)
	{
		const auto& embedded = *ptr;
		const auto& element = embedded.element;
		const auto& text = embedded.text;

		math::transformf offset;
		offset.translate(element.rect.x, element.rect.y, 0);
		add_text(text, transform * offset);
	}
	auto sorted_images = t.get_embedded_images();

	std::sort(std::begin(sorted_images), std::end(sorted_images), [](const auto& lhs, const auto& rhs)
	{
		return lhs->data.image.lock() < rhs->data.image.lock();
	});

	for(const auto& ptr : sorted_images)
	{
		const auto& embedded = *ptr;
		const auto& element = embedded.element;
		auto image = embedded.data.image.lock();

		const auto& img_src_rect = embedded.data.src_rect;
        rect dst_rect = {int(element.rect.x), int(element.rect.y),
                         int(element.rect.w), int(element.rect.h)};
		add_image(image, img_src_rect, dst_rect, transform);
	}
}

void draw_list::add_text(const rich_text& t, const math::transformf& transform, const rect& dst_rect, size_fit sz_fit,
						 dimension_fit dim_fit)
{
	add_text(t, align_and_fit_text(t, transform, dst_rect, sz_fit, dim_fit));
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
    const auto& lines = t.get_lines_metrics();

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

    if(!line_path.empty())
    {
        push_transform(transform);
        add_polyline(line_path, color::yellow(), false);
        pop_transform();
    }
    else
    {
        math::transformf tr = transform;
        {
            for(const auto& line : lines)
            {
                auto v1 = transform.transform_coord({line.minx, line.ascent});
                auto v2 = transform.transform_coord({line.maxx, line.ascent});

                text txt;
                txt.set_color(color::cyan());
                txt.set_font(default_font());
                txt.set_alignment(align::right | align::middle);
                txt.set_utf8_text("ascent ");

                tr.set_position(v1.x, v1.y, 0.0f);
                add_text(txt, tr);

                auto width = line.maxx - line.minx;
                std::stringstream ss;
                ss << " width = " << std::fixed << std::setprecision(2) << width;

                txt.set_alignment(align::left | align::middle);
                txt.set_utf8_text(ss.str());

                tr.set_position(v2.x, v1.y, 0.0f);
                add_text(txt, tr);
            }
        }
        {
            for(const auto& line : lines)
            {
                auto v1 = transform.transform_coord({line.minx, line.cap_height});

                text txt;
                txt.set_color(color::green());
                txt.set_font(default_font());
                txt.set_alignment(align::right | align::middle);
                txt.set_utf8_text("cap_height ");

                tr.set_position(v1.x, v1.y, 0.0f);
                add_text(txt, tr);
            }
        }
        {
            for(const auto& line : lines)
            {
                auto v1 = transform.transform_coord({line.minx, line.median});

                text txt;
                txt.set_color(color::red());
                txt.set_font(default_font());
                txt.set_alignment(align::right | align::middle);
                txt.set_utf8_text("median ");

                tr.set_position(v1.x, v1.y, 0.0f);
                add_text(txt, tr);
            }
        }
        {
            for(const auto& line : lines)
            {
                auto v1 = transform.transform_coord({line.minx, line.baseline});

                text txt;
                txt.set_color(color::magenta());
                txt.set_font(default_font());
                txt.set_alignment(align::right | align::middle);
                txt.set_utf8_text("baseline ");

                tr.set_position(v1.x, v1.y, 0.0f);
                add_text(txt, tr);
            }
        }
        {
            for(const auto& line : lines)
            {
                auto v1 = transform.transform_coord({line.minx, line.descent});

                text txt;
                txt.set_color(color::blue());
                txt.set_font(default_font());
                txt.set_alignment(align::right | align::middle);
                txt.set_utf8_text("descent ");

                tr.set_position(v1.x, v1.y, 0.0f);
                add_text(txt, tr);
            }
        }



        add_rect(t.get_frect(), transform, color::red(), false, 3.0f);

        for(const auto& line : lines)
        {
            math::vec2 v1{line.minx, line.ascent};
            math::vec2 v2{line.maxx, line.ascent};

            add_line(transform.transform_coord(v1), transform.transform_coord(v2), color::cyan());
        }

        for(const auto& line : lines)
        {
            math::vec2 v1{line.minx, line.cap_height};
            math::vec2 v2{line.maxx, line.cap_height};

            add_line(transform.transform_coord(v1), transform.transform_coord(v2),  color::green());
        }

        for(const auto& line : lines)
        {
            math::vec2 v1{line.minx, line.x_height};
            math::vec2 v2{line.maxx, line.x_height};

            add_line(transform.transform_coord(v1), transform.transform_coord(v2), color::magenta());
        }

        for(const auto& line : lines)
        {
            math::vec2 v1{line.minx, line.median};
            math::vec2 v2{line.maxx, line.median};

            add_line(transform.transform_coord(v1), transform.transform_coord(v2), color::red());
        }

        for(const auto& line : lines)
        {
            math::vec2 v1{line.minx, line.baseline};
            math::vec2 v2{line.maxx, line.baseline};

            add_line(transform.transform_coord(v1), transform.transform_coord(v2), color::magenta());
        }

        for(const auto& line : lines)
        {
            math::vec2 v1{line.minx, line.descent};
            math::vec2 v2{line.maxx, line.descent};

            add_line(transform.transform_coord(v1), transform.transform_coord(v2), color::blue());
        }

		auto font = t.get_style().font;
		if(font)
		{
			for(const auto& line : lines)
			{
				auto line_height = font->line_height;
				math::vec2 v1{line.maxx, line.ascent};
				math::vec2 v2{line.maxx, line.ascent + line_height};

				add_line(transform.transform_coord(v1), transform.transform_coord(v2), color::green());
			}
		}
    }

    add_rect(rect{-1, -1, 2, 2}, transform, color{255, 190, 2});
}


bool set_debug_draw(bool debug)
{
	bool old = debug_draw_enabled;
	debug_draw_enabled = debug;
	return old;
}

void toggle_debug_draw()
{
	debug_draw_enabled = !debug_draw_enabled;
}

bool debug_draw()
{
	return debug_draw_enabled;
}


}
