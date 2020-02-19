#include "text.h"
#include "font.h"

#include <array>
#include <algorithm>
#include <iostream>
namespace gfx
{


namespace
{

constexpr size_t vertices_per_quad = 4;


// line feed (0x0a, '\n')
bool is_newline(uint32_t c)
{
    return c == 0x0a;
}

// Checks if the given character is a blank character
// as classified by the currently installed C locale.
// Blank characters are whitespace characters used to
// separate words within a sentence.
// In the default C locale, only
// horizontal tab (0x09, '\t')
// space (0x20, ' ')
// are classified as blank characters.
bool is_blank(uint32_t c)
{
    return c == 0x20 || c == 0x09;
}

// Checks if the given character is whitespace character as classified by the currently installed C locale.
// In the default locale, the whitespace characters are the following:
// horizontal tab (0x09, '\t')
// space (0x20, ' ')
// line feed (0x0a, '\n')
// vertical tab (0x0b, '\v')
// form feed (0x0c, '\f')
// carriage return (0x0d, '\r')
bool is_space(uint32_t c)
{
    return is_blank(c) ||is_newline(c) || c == 0x0b || c == 0x0c || c == 0x0d;
}

inline color get_gradient(const math::vec4& ct, const math::vec4& cb, float t, float dt)
{
    const float step = std::min(std::max(t, 0.0f), dt);
    const float fa = step/dt;
    const auto c = math::lerp(ct, cb, fa);
    return {
        uint8_t(c.r),
        uint8_t(c.g),
        uint8_t(c.b),
        uint8_t(c.a)
    };
}

bool apply_line_path(std::array<vertex_2d, 4>& quad,
                     const polyline& line_path,
                     float minx,
                     float curr_dist,
                     float leaning0,
                     float leaning1)
{
    minx = std::min(minx, 0.0f);
    if(line_path.empty())
    {
        quad[0].pos.x += leaning0;
        quad[1].pos.x += leaning0;
        quad[2].pos.x += leaning1;
        quad[3].pos.x += leaning1;

        return true;
    }

    const auto& path_points = line_path.get_points();
    float point_dist = 0;
    int point_idx = line_path.get_closest_point(curr_dist, point_dist);
    if(point_idx < 0)
    {
        return false;
    }

    const auto closest_point = path_points[size_t(point_idx)];
    const auto next_point = path_points[size_t(point_idx) + 1];
    const auto segment_direction = math::normalize(next_point - closest_point);

    const math::vec3 z_axis{0.0f, 0.0f, 1.0f};
    const math::vec3 y_axis{0.0f, 1.0f, 0.0f};
    const math::vec3 normal = math::normalize(math::cross(z_axis, math::vec3(segment_direction, 0.0f)));
    auto angle = std::atan2(y_axis.x * normal.y - y_axis.y * normal.x, y_axis.x * normal.x + y_axis.y * normal.y);

    const float offset = math::abs(curr_dist - point_dist);
    const auto pos_on_line = closest_point + (segment_direction * offset);

    const auto w = (quad[1].pos.x - quad[0].pos.x);

    math::transformf rotation;
    rotation.rotate_local({0, 0, angle});
    math::transformf translation;
    translation.translate(pos_on_line.x, pos_on_line.y, 0.0f);
    const auto transformation = translation * rotation * math::inverse(translation);

    for(size_t i = 0; i < quad.size(); ++i)
    {
        auto& p = quad[i];
        p.pos.x = pos_on_line.x + minx;
        p.pos.y += pos_on_line.y;

        if(i == 1 || i == 2)
        {
            p.pos.x += w;
        }

        if(i < 2)
        {
            p.pos.x += leaning0;
        }
        else
        {
            p.pos.x += leaning1;
        }

        p.pos = transformation.transform_coord(p.pos);
    }

    return true;
}
}


float get_alignment_x(align_t alignment, float minx, float maxx, bool pixel_snap)
{
    float xoffs = 0;

    if(alignment & align::left)
    {
        xoffs = -minx;
    }

    if(alignment & align::right)
    {
        xoffs = -maxx;
    }

    if(alignment & align::center)
    {
        xoffs = (-minx-maxx) / 2.0f;
    }

    if(pixel_snap)
    {
        xoffs = float(int(xoffs));
    }

    return xoffs;
}

float get_alignment_y(align_t alignment,
                      float miny, float miny_baseline, float miny_cap,
                      float maxy, float maxy_baseline, float maxy_cap, bool /*pixel_snap*/)
{
    float yoffs = 0;

    if(alignment & align::top)
    {
        yoffs = -miny;
    }

    if(alignment & align::cap_height_top)
    {
        yoffs = -miny_cap;
    }

    if(alignment & align::baseline_top)
    {
        yoffs = -miny_baseline;
    }

    if(alignment & align::bottom)
    {
        yoffs = -maxy;
    }

    if(alignment & align::baseline_bottom)
    {
        yoffs = -maxy_baseline;
    }

    if(alignment & align::cap_height_bottom)
    {
        yoffs = -maxy_cap;
    }

    if(alignment & align::middle)
    {
        yoffs = (-miny -maxy) / 2.0f;
    }

    return yoffs;
}


float get_alignment_y(align_t alignment,
					  float miny,
					  float maxy, bool pixel_snap)
{
	return get_alignment_y(alignment, miny, miny, miny, maxy, maxy, maxy, pixel_snap);
}

std::pair<float, float> get_alignment_offsets(
	align_t alignment,
	float minx, float miny, float miny_baseline, float miny_cap,
	float maxx, float maxy, float maxy_baseline, float maxy_cap,
	bool pixel_snap)
{
	return {get_alignment_x(alignment, minx, maxx, pixel_snap),
			get_alignment_y(alignment, miny, miny_baseline, miny_cap, maxy, maxy_baseline, maxy_cap, pixel_snap)};
}

std::pair<float, float> get_alignment_offsets(
	align_t alignment,
	float minx, float miny,
	float maxx, float maxy,
	bool pixel_snap)
{
	return get_alignment_offsets(alignment, minx, miny, miny, miny, maxx, maxy, maxy, maxy, pixel_snap);

}

text::~text()
{
    cache<text>::add(geometry_);
    for(auto& line : lines_)
    {
        cache<text>::add(line);
    }
    cache<text>::add(lines_);
    cache<text>::add(lines_metrics_);
    cache<text>::add(unicode_text_);
    cache<text>::add(utf8_text_);
}

void text::set_utf8_text(const std::string& t)
{
    if(t == utf8_text_)
    {
        return;
    }
    utf8_text_ = t;
    clear_lines();
    clear_geometry();
}

void text::set_utf8_text(std::string&& t)
{
    if(t == utf8_text_)
    {
        return;
    }
    utf8_text_ = std::move(t);
    clear_lines();
    clear_geometry();
}

void text::set_font(const font_ptr& f)
{
    if(font_ == f)
    {
        return;
    }
    font_ = f;
    clear_lines();
    clear_geometry();
}

const font_ptr& text::get_font() const
{
    return font_;
}

align_t text::get_alignment() const
{
    return alignment_;
}

void text::set_color(color c)
{
    set_vgradient_colors(c, c);
}

void text::set_vgradient_colors(color top, color bot)
{
    if(decorator_.color_top == top && decorator_.color_bot == bot)
    {
        return;
    }
    decorator_.color_top = top;
    decorator_.color_bot = bot;

    for(auto& decorator : decorators_)
    {
        decorator.color_top = top;
        decorator.color_bot = bot;
    }

    clear_geometry();
}

void text::set_outline_color(color c)
{
    if(outline_color_ == c)
    {
        return;
    }
    outline_color_ = c;
}

void text::set_outline_width(float owidth)
{
    owidth = std::min(0.4f, owidth);
    if(math::epsilonEqual(outline_width_, owidth, math::epsilon<float>()))
    {
        return;
    }
    outline_width_ = std::max(0.0f, owidth);
    clear_lines();
    clear_geometry();
}

void text::set_shadow_color(color c)
{
    set_shadow_vgradient_colors(c, c);
}

void text::set_shadow_vgradient_colors(color top, color bot)
{
    if(shadow_color_top_ == top && shadow_color_bot_ == bot)
    {
        return;
    }
    shadow_color_top_ = top;
    shadow_color_bot_ = bot;
}

void text::set_shadow_offsets(const math::vec2& offsets)
{
    if(math::all(math::epsilonEqual(offsets, shadow_offsets_, math::epsilon<float>())))
    {
        return;
    }

    shadow_offsets_ = offsets;
    clear_lines();
    clear_geometry();
}

void text::set_advance(const math::vec2& advance)
{
    if(math::all(math::epsilonEqual(advance, decorator_.advance, math::epsilon<float>())))
    {
        return;
    }

    decorator_.advance = advance;
    clear_lines();
    clear_geometry();
}


void text::set_alignment(align_t a)
{
    if(alignment_ == a)
    {
        return;
    }
    alignment_ = a;
    clear_geometry();
}

const std::vector<vertex_2d>& text::get_geometry() const
{
    if(geometry_.empty())
    {
        update_geometry(true);
    }
    return geometry_;
}

const std::vector<std::vector<uint32_t>>& text::get_lines() const
{
    update_lines();
    return lines_;
}

const std::vector<uint32_t> &text::get_unicode_text() const
{
    update_unicode_text();
    return unicode_text_;
}

const std::vector<line_metrics>& text::get_lines_metrics() const
{
    get_geometry();
    return lines_metrics_;
}

bool text::is_valid() const
{
    return !utf8_text_.empty() && font_;
}

void text::set_align_line_callback(const std::function<void (size_t, float)> &callback)
{
    align_line_callback = callback;
}

void text::set_clear_geometry_callback(const std::function<void ()> &callback)
{
	clear_geometry_callback = callback;
}


float text::get_advance_offset_y(const text_decorator& decorator) const
{
    if(font_ && font_->sdf_spread > 0)
    {
        return decorator.advance.y + (outline_width_ * float(font_->sdf_spread + 3) * 2.0f);
    }

    return decorator.advance.y;
}

void text::set_kerning(bool enabled)
{
    if(decorator_.kerning_enabled == enabled)
    {
        return;
    }
    decorator_.kerning_enabled = enabled;
    clear_geometry();
}

void text::set_leaning(float leaning)
{
    leaning = math::clamp(leaning, -45.0f, 45.0f);
    if(math::epsilonEqual(decorator_.leaning, leaning, math::epsilon<float>()))
    {
        return;
    }
    decorator_.leaning = leaning;
    clear_geometry();
}

float text::get_advance_offset_x(const text_decorator& decorator) const
{
    if(font_ && font_->sdf_spread > 0)
    {
        return decorator.advance.x + (outline_width_ * float(font_->sdf_spread + 3) * 2.0f);
    }

    return decorator.advance.x;
}

void text::set_max_width(float max_width)
{
    if(math::epsilonEqual(max_width_, max_width, math::epsilon<float>()))
    {
        return;
    }
    max_width_ = max_width;
    clear_lines();
    clear_geometry();
}

void text::set_line_path(const polyline& line)
{
    line_path_ = line;
    clear_geometry();
}

void text::set_line_path(polyline&& line)
{
    line_path_ = std::move(line);
    clear_geometry();
}

void text::set_decorators(const std::vector<text_decorator>& decorators)
{
    decorators_ = decorators;
    clear_geometry();
}
void text::set_decorators(std::vector<text_decorator>&& decorators)
{
    decorators_ = std::move(decorators);
    clear_geometry();
}
void text::add_decorator(const text_decorator& decorator)
{
    decorators_.emplace_back(decorator);
    clear_geometry();
}
void text::add_decorator(text_decorator&& decorator)
{
    decorators_.emplace_back(decorator);
    clear_geometry();
}
const polyline& text::get_line_path() const
{
    return line_path_;
}

const std::string& text::get_utf8_text() const
{
    return utf8_text_;
}

rect cast_rect(const frect& r)
{
    rect result;
    result.x = static_cast<int>(r.x);
    result.y = static_cast<int>(r.y);
    result.w = static_cast<int>(r.w);
    result.h = static_cast<int>(r.h);
    return result;
}

void text::clear_geometry()
{
    geometry_.clear();
    lines_metrics_.clear();
    rect_ = {};

    if(clear_geometry_callback)
    {
        clear_geometry_callback();
    }
}

void text::clear_lines()
{
    chars_ = 0;
    lines_.clear();
    unicode_text_.clear();
}

void text::update_lines() const
{
    // if we already generated lines
    if(!lines_.empty())
    {
        return;
    }

    if(!font_)
    {
        return;
    }

    const auto& unicode_text = get_unicode_text();

    if(unicode_text.empty())
    {
        return;
    }
    // find newlines
    float advance = 0.0f;
    auto last_space = size_t(-1);

    lines_.clear();
    cache<text>::get(lines_, 1);
    lines_.resize(1);

    const auto unicode_text_size = unicode_text.size();
    cache<text>::get(lines_.back(), unicode_text_size);
    lines_.back().reserve(unicode_text_size);

    auto max_width = float(max_width_) - font_->get_glyph(' ').advance_x;

    auto decorator = &decorator_;
    auto next_decorator = get_next_decorator(0, decorator);

    auto advance_offset_x = get_advance_offset_x(*decorator);

    for(size_t i = 0; i < unicode_text_size; ++i)
    {
        uint32_t c = unicode_text[i];

        // check if is a break possible character
        if(is_space(c))
        {
            last_space = i;
        }

        const auto& g = font_->get_glyph(c);
        auto glyph_advance = g.advance_x;

        float callback_advance = 0.0f;

        // decorator begin
        {
            if(get_decorator(chars_, decorator, next_decorator))
            {
                advance_offset_x = get_advance_offset_x(*decorator);
            }

			if(decorator->calculate_size && (decorator->match_range.end - 1) == chars_)
			{
				const char* str_begin = utf8_text_.data() + decorator->utf8_range.begin;
				const char* str_end = utf8_text_.data() + decorator->utf8_range.end;
				callback_advance = decorator->calculate_size(str_begin, str_end);
            }

			if(decorator->calculate_size || !decorator->visual_range.contains(chars_))
            {
                glyph_advance = 0;
            }

            glyph_advance *= decorator->scale;
            glyph_advance += callback_advance + advance_offset_x;
        }
        // decorator end

        bool exceedsmax_width = max_width > 0 && (advance + glyph_advance) > max_width;

		if(is_newline(c) || (exceedsmax_width && (last_space != size_t(-1))))
        {
            if(is_blank(c) || is_newline(c))
            {
                lines_.back().push_back(c);
                ++chars_;
            }

            if(i > last_space)
            {
                lines_.back().resize(lines_.back().size() - (i - (last_space + 1)));
                chars_ -= uint32_t(i - (last_space + 1));
                i = last_space;
            }

            lines_.resize(lines_.size() + 1);
            cache<text>::get(lines_.back(), unicode_text_size - chars_);
            lines_.back().reserve(unicode_text_size - chars_);
            advance = 0;
            last_space = size_t(-1);
        }
		else
        {
            lines_.back().push_back(c);
            ++chars_;
            advance += glyph_advance;
        }
    }
}


void text::update_unicode_text() const
{
    // we already generated unicode codepoints?
    if(!unicode_text_.empty())
    {
        return;
    }

    if(utf8_text_.empty())
    {
        return;
    }

    cache<text>::get(unicode_text_, utf8_text_.size() * 2);
    unicode_text_.reserve(utf8_text_.size() * 2); // enough for most scripts

    const char* beg = utf8_text_.c_str();
    const char* const end = beg + utf8_text_.size();

    while(beg < end)
    {
        uint32_t uchar{};

        int m = fnt::text_char_from_utf8(&uchar, beg, end);
        if(!m)
        {
            break;
        }
        unicode_text_.push_back(uchar);
        beg += m;
    }
}

const text_decorator* text::get_next_decorator(size_t glyph_idx, const text_decorator* current) const
{
    const text_decorator* result = &decorator_;

    for(const auto& decorator : decorators_)
    {
        if(current->match_range.end >= glyph_idx && current != &decorator && glyph_idx <= decorator.match_range.begin)
        {
            if(result == &decorator_ || decorator.match_range.begin < result->match_range.begin)
            {
                result = &decorator;
            }
        }
    }

    return result;
}

bool text::get_decorator(size_t i, const text_decorator*& current, const text_decorator*& next) const
{
    bool changed = false;

    if(i >= current->match_range.end)
    {
        if(current != next && i >= next->match_range.begin)
        {
            changed = true;
            current = next;
            next = get_next_decorator(i, current);
        }
        else if(current != &decorator_)
        {
            changed = true;
            current = &decorator_;
        }
    }
    return changed;
}

void text::update_geometry(bool all) const
{
    if(!font_)
    {
        return;
    }

    if(!geometry_.empty())
    {
        return;
    }
    const auto& lines = get_lines();
    if(lines.empty())
    {
        return;
    }

    auto decorator = &decorator_;
    auto next_decorator = get_next_decorator(0, decorator);

    auto advance_offset_x = get_advance_offset_x(*decorator);
    auto advance_offset_y = get_advance_offset_y(*decorator);

    auto color_top = decorator->color_top;
    auto color_bot = decorator->color_bot;
    math::vec4 vcolor_top{color_top.r, color_top.g, color_top.b, color_top.a};
    math::vec4 vcolor_bot{color_bot.r, color_bot.g, color_bot.b, color_bot.a};
    bool has_gradient = color_top != color_bot;
    bool kerning_enabled = decorator->kerning_enabled;

    float leaning = 0.0f;
    bool has_leaning = false;
    auto line_height = font_->line_height + advance_offset_y;
    const auto pixel_snap = font_->pixel_snap;
    const auto ascent = font_->ascent;
    const auto descent = font_->descent;
    const auto height = ascent - descent;

    const auto line_gap = line_height - height;
    const auto x_height = font_->x_height;
    const auto cap_height = font_->cap_height;
    const auto median = cap_height * 0.5f;
    if(all)
    {
        cache<text>::get(lines_metrics_, lines.size());
        lines_metrics_.reserve(lines.size());
        cache<text>::get(geometry_, chars_ * vertices_per_quad);
        geometry_.resize(chars_ * vertices_per_quad);

        has_leaning = math::epsilonNotEqual(decorator->leaning, 0.0f, math::epsilon<float>());
        if(has_leaning)
        {
            leaning = math::rotateZ(math::vec3{0.0f, ascent, 0.0f}, math::radians(-decorator->leaning)).x;
        }
    }

    size_t offset = 0;
    auto vptr = geometry_.data();

    float max_h = (lines.size() * line_height) - line_gap;
    float min_y = 0.0f;
    float max_y = max_h;
    float min_y_baseline = min_y + ascent;
    float max_y_baseline = max_y + descent;
    float min_y_cap = min_y_baseline - cap_height;
    float max_y_cap = max_y_baseline - cap_height;

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();

    float align_y = get_alignment_y(alignment_,
                                    min_y, min_y_baseline, min_y_cap,
                                    max_y, max_y_baseline, max_y_cap,
                                    pixel_snap);

    min_y += align_y;
    max_y += align_y;

    // Set glyph positions on a (0,0) baseline.
    // (x0,y0) for a glyph is the bottom-lefts
    auto pen_x = 0.0f;
    auto pen_y = ascent + align_y;

    size_t glyphs_processed = 0;
    size_t glyhps_to_render = 0;
    for(const auto& line : lines)
    {
        line_metrics metrics{};
        metrics.minx = pen_x;
        metrics.ascent = pen_y - ascent;
        metrics.cap_height = pen_y - cap_height;
        metrics.x_height = pen_y - x_height;
        metrics.median = pen_y - cap_height * 0.5f;
        metrics.baseline = pen_y;
        metrics.descent = pen_y - descent;

        size_t vtx_count{};
        auto last_codepoint = char_t(-1);

        for(auto c : line)
        {
            const auto& g = font_->get_glyph(c);

            if(is_newline(c))
            {
                glyphs_processed++;
                continue;
            }

            auto pen_y_decorated = pen_y;

            // decorator begin
            {
                if(get_decorator(glyphs_processed, decorator, next_decorator))
                {
                    advance_offset_x = get_advance_offset_x(*decorator);
                    kerning_enabled = decorator->kerning_enabled;

                    if(all)
                    {
                        color_top = decorator->color_top;
                        color_bot = decorator->color_bot;
                        vcolor_top = {color_top.r, color_top.g, color_top.b, color_top.a};
                        vcolor_bot = {color_bot.r, color_bot.g, color_bot.b, color_bot.a};
                        has_gradient = color_top != color_bot;

                        has_leaning = math::epsilonNotEqual(decorator->leaning, 0.0f, math::epsilon<float>());
                        if(has_leaning)
                        {
                            leaning = math::rotateZ(math::vec3{0.0f, ascent, 0.0f}, math::radians(-decorator->leaning)).x;
                        }
                    }
                }

				if((decorator->match_range.end - 1) == glyphs_processed)
				{
					const char* str_begin = utf8_text_.data() + decorator->utf8_range.begin;
					const char* str_end = utf8_text_.data() + decorator->utf8_range.end;

					float external_sz = 0.0f;
					if(decorator->calculate_size)
					{
						external_sz = decorator->calculate_size(str_begin, str_end);
					}

					if(all)
					{
						if(decorator->generate_geometry)
						{
							decorator->generate_geometry(pen_x, pen_y, lines_metrics_.size(), str_begin, str_end);
						}
					}

					pen_x += external_sz;
                }

				if(decorator->generate_geometry || !decorator->visual_range.contains(glyphs_processed))
                {
                    glyphs_processed++;
                    continue;
                }
            }

            auto scale = std::max(decorator->scale, 0.001f);

            if(all)
            {
                // ----------------------------
                // Apply the decorator
                // ----------------------------
                switch(decorator->script)
                {
                    case script_line::ascent:
                        pen_y_decorated = metrics.ascent + ascent * scale;
                    break;
                    case script_line::cap_height:
                        pen_y_decorated = metrics.cap_height + cap_height * scale;
                    break;
                    case script_line::x_height:
                        pen_y_decorated = metrics.x_height + x_height * scale;
                    break;
                    case script_line::median:
                        pen_y_decorated = metrics.median + median * scale;
                    break;
                    case script_line::baseline:
                        pen_y_decorated = metrics.baseline;
                    break;
                    case script_line::descent:
                        pen_y_decorated = metrics.descent + descent * scale;
                    break;
                    default:
                    break;
                }

                // ----------------------------
            }


            if(kerning_enabled)
            {
                // modify the pen advance with the kerning from the previous character
                pen_x += font_->get_kerning(last_codepoint, g.codepoint) * scale;
                last_codepoint = g.codepoint;
            }


            if(all)
            {
                float leaning0 = 0.0f;
                float leaning1 = 0.0f;

                if(has_leaning)
                {
                    const auto y0_offs = g.y0 * scale + ascent;
                    const auto y0_factor = 1.0f - y0_offs / ascent;
                    leaning0 = leaning * y0_factor;

                    const auto y1_offs = g.y1 * scale + ascent;
                    const auto y1_factor = 1.0f - y1_offs / ascent;
                    leaning1 = leaning * y1_factor;
                }


                auto x0 = pen_x + g.x0 * scale;
                auto x1 = pen_x + g.x1 * scale;
                auto y0 = pen_y_decorated + g.y0 * scale;
                auto y1 = pen_y_decorated + g.y1 * scale;

                const auto coltop = has_gradient ? get_gradient(vcolor_top, vcolor_bot, y0 - metrics.ascent, height) : color_top;
                const auto colbot = has_gradient ? get_gradient(vcolor_top, vcolor_bot, y1 - metrics.ascent, height) : color_bot;

                std::array<vertex_2d, 4> quad =
                {{
                    {{x0, y0}, {g.u0, g.v0}, coltop},
                    {{x1, y0}, {g.u1, g.v0}, coltop},
                    {{x1, y1}, {g.u1, g.v1}, colbot},
                    {{x0, y1}, {g.u0, g.v1}, colbot}
                }};

                if(!apply_line_path(quad, line_path_, x0, pen_x, leaning0, leaning1))
                {
                    break;
                }
                std::memcpy(vptr, quad.data(), quad.size() * sizeof(vertex_2d));
                vptr += vertices_per_quad;
                vtx_count += vertices_per_quad;
                glyhps_to_render++;
            }

            pen_x += advance_offset_x + g.advance_x * scale;

            glyphs_processed++;
        }

        metrics.maxx = pen_x;

        if(all)
        {
            auto align_x = get_alignment_x(alignment_,
                                           metrics.minx,
                                           metrics.maxx,
                                           pixel_snap);

            if(line_path_.empty() && (math::epsilonNotEqual(align_x, 0.0f, math::epsilon<float>()) || pixel_snap))
            {
                auto v = geometry_.data() + offset;
                for(size_t i = 0; i < vtx_count; ++i)
                {
                    auto& vv = *(v+i);
                    vv.pos.x += align_x;

                    if(pixel_snap)
                    {
                        vv.pos.x = float(int(vv.pos.x));
                    }
                }

                if(align_line_callback)
                {
                    align_line_callback(lines_metrics_.size(), align_x);
                }
            }

            metrics.minx += align_x;
            metrics.maxx += align_x;

            lines_metrics_.emplace_back(metrics);

            offset += vtx_count;
        }

        min_x = std::min(metrics.minx, min_x);
        max_x = std::max(metrics.maxx, max_x);

        // go to next line
        pen_x = 0;
        pen_y += line_height;
    }

    if(all)
    {
        rect_.x = min_x;
        rect_.y = min_y;

        geometry_.resize(glyhps_to_render * vertices_per_quad);
    }

    rect_.h = max_y - min_y;
    rect_.w = max_x - min_x;
}

float text::get_width() const
{
    if(rect_)
    {
        return rect_.w;
    }

    update_geometry(false);

    return rect_.w;
}

float text::get_height() const
{
    if(rect_)
    {
        return rect_.h;
    }

    update_geometry(false);

    return rect_.h;
}

float text::get_min_baseline_height() const
{
    const auto& metrics = get_lines_metrics();

    if(metrics.empty())
    {
        return 0.0f;
    }

    const auto& first_line = metrics.front();
    return first_line.baseline - first_line.ascent;
}

float text::get_max_baseline_height() const
{
    const auto& metrics = get_lines_metrics();

    if(metrics.empty())
    {
        return 0.0f;
    }

    const auto& first_line = metrics.front();
    const auto& last_line = metrics.back();
    return last_line.baseline - first_line.ascent;
}

rect text::get_rect() const
{
    return cast_rect(get_frect());
}

const frect& text::get_frect() const
{
	update_geometry(true);

    return rect_;
}

color text::get_outline_color() const
{
    if(outline_width_ > 0.0f)
    {
        return outline_color_;
    }

    return {0, 0, 0, 0};
}

float text::get_outline_width() const
{
    return outline_width_;
}

const math::vec2& text::get_shadow_offsets() const
{
    return shadow_offsets_;
}

color text::get_shadow_color_top() const
{
    return shadow_color_top_;
}

color text::get_shadow_color_bot() const
{
    return shadow_color_bot_;
}

std::vector<text_decorator*> text::add_decorators(const std::string& style_id)
{
	const std::string braces_matcher = R"(\(([\s\S]*?)\))";
	const std::string match_style = style_id + braces_matcher;

	const std::regex global_rx(match_style); // --> style_id(some text)
	const std::regex local_rx(braces_matcher); // --> (some text)

	return add_decorators(global_rx, local_rx);
}
std::vector<text_decorator*> text::add_decorators(const std::regex& matcher, const std::regex& visual_matcher)
{
    auto sz_before = decorators_.size();

    const auto& text = get_utf8_text();

    for(auto it = std::sregex_iterator(text.begin(), text.end(), matcher);
        it != std::sregex_iterator();
         ++it)
    {

        auto idx = it->position();
        auto sz = it->length();

        auto begin = text.data() + idx;
        auto end = begin + sz;

		decorators_.emplace_back(get_decorator());
        auto& decorator = decorators_.back();

		decorator.match_range.begin = count_glyphs(text.data(), begin);
		decorator.match_range.end = decorator.match_range.begin + count_glyphs(begin, end);

		decorator.visual_range = decorator.match_range;

		decorator.utf8_range.begin = idx;
		decorator.utf8_range.end = decorator.utf8_range.begin + sz;

		auto begin_it = std::begin(text) + idx;
		auto end_it = begin_it + sz;

		std::string str(begin_it, end_it);
		for(auto local_it = std::sregex_iterator(begin_it, end_it, visual_matcher);
			local_it != std::sregex_iterator();
			++local_it)
		{
			auto local_idx = local_it->position();
			auto local_sz = local_it->length();

			auto local_begin = begin + local_idx;
			auto local_end = local_begin + local_sz;

			decorator.visual_range.begin = count_glyphs(text.data(), local_begin);
			decorator.visual_range.end = decorator.visual_range.begin + count_glyphs(local_begin, local_end);

			decorator.utf8_range.begin = idx + local_idx;
			decorator.utf8_range.end = decorator.utf8_range.begin + local_sz;


			std::smatch pieces_match;
			if (std::regex_match(begin_it + local_idx, begin_it + local_idx + local_sz, pieces_match, visual_matcher))
			{
				// The first sub_match is the whole string; the next
				// sub_match is the first parenthesized expression.
				if (pieces_match.size() == 2)
				{
					std::ssub_match base_sub_match = pieces_match[1];
					local_idx = std::distance(begin_it, base_sub_match.first);
					local_sz = base_sub_match.length();

					local_begin = begin + local_idx;
					local_end = local_begin + local_sz;

					decorator.visual_range.begin = count_glyphs(text.data(), local_begin);
					decorator.visual_range.end = decorator.visual_range.begin + count_glyphs(local_begin, local_end);

					decorator.utf8_range.begin = idx + local_idx;
					decorator.utf8_range.end = decorator.utf8_range.begin + local_sz;
				}
			}

		}

    }

    if(!decorators_.empty())
    {
        auto& decorator = decorators_.back();
        if(!decorator.match_range.end)
        {
            decorators_.pop_back();
        }
    }

    auto sz_after = decorators_.size();

    auto count = sz_after - sz_before;

    std::vector<text_decorator*> result;
    if(count > 0)
    {
        result.resize(count);
        for(size_t i = 0; i < count; ++i)
        {
            result[i] = &decorators_[sz_before + i];
        }
    }

    clear_geometry();

    return result;
}

size_t text::count_glyphs(const std::string &utf8_text)
{
    const char* beg = utf8_text.c_str();
    const char* const end = beg + utf8_text.size();
    return count_glyphs(beg, end);
}

size_t text::count_glyphs(const char* beg, const char* end)
{
    size_t count{};
    while(beg < end)
    {
        uint32_t uchar{};

        int m = fnt::text_char_from_utf8(&uchar, beg, end);
        if(!m)
        {
            break;
        }
        count++;
        beg += m;
    }

    return count;
}

const text_decorator& text::get_decorator() const
{
	return decorator_;
}

}
