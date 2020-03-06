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

rect cast_rect(const frect& r)
{
	rect result;
	result.x = static_cast<int>(r.x);
	result.y = static_cast<int>(r.y);
	result.w = static_cast<int>(r.w);
	result.h = static_cast<int>(r.h);
	return result;
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

bool text_decorator::range::contains(size_t idx) const
{
	if(end == 0)
	{
		return true;
	}
	return idx >= begin && idx < end;
}

bool text_decorator::range::at_end(size_t idx) const
{
	return idx == end - 1;
}

bool text_decorator::range::empty() const
{
	return (begin == end) == 0;
}

text::~text()
{
	cache<text>::add(geometry_);
	for(auto& line : lines_)
	{
		cache<text>::add(line);
	}
	cache<text>::add(lines_);
	cache<text>::add(unicode_text_);
	cache<text>::add(utf8_text_);

}

bool text::set_utf8_text(const std::string& t)
{
	if(t == utf8_text_)
	{
		return false;
	}
	utf8_text_ = t;
	clear_lines();
	decorators_.clear();

	return true;
}

bool text::set_utf8_text(std::string&& t)
{
	if(t == utf8_text_)
	{
		return false;
	}
	utf8_text_ = std::move(t);
	clear_lines();
	decorators_.clear();

	return true;
}

void text::set_style(const text_style& style)
{
    main_decorator_.scale = style.scale;
	style_ = style;
	clear_lines();
}

void text::set_font(const font_ptr& f)
{
	if(style_.font == f)
	{
		return;
	}
	style_.font = f;
	clear_lines();
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
	if(style_.color_top == top && style_.color_bot == bot)
	{
		return;
	}
	style_.color_top = top;
	style_.color_bot = bot;

	clear_geometry();
}

void text::set_outline_vgradient_colors(color top, color bot)
{
	if(style_.outline_color_top == top && style_.outline_color_bot == bot)
	{
		return;
	}
	style_.outline_color_top = top;
	style_.outline_color_bot = bot;

	clear_geometry();
}

void text::set_outline_color(color c)
{
	set_outline_vgradient_colors(c, c);
}

void text::set_outline_width(float owidth)
{
	owidth = math::clamp(owidth, 0.0f, 0.4f);
	if(math::epsilonEqual(style_.outline_width, owidth, math::epsilon<float>()))
	{
		return;
	}
	style_.outline_width = owidth;
	clear_lines();
}

void text::set_outline_softness(float softness)
{
	softness = math::clamp(softness, 0.0f, 1.0f);
	if(math::epsilonEqual(style_.outline_softness, softness, math::epsilon<float>()))
	{
		return;
	}
	style_.outline_softness = softness;
	clear_lines();
}

void text::set_shadow_color(color c)
{
	set_shadow_vgradient_colors(c, c);
}

void text::set_shadow_vgradient_colors(color top, color bot)
{
	if(style_.shadow_color_top == top && style_.shadow_color_bot == bot)
	{
		return;
	}
	style_.shadow_color_top = top;
	style_.shadow_color_bot = bot;
}

void text::set_shadow_offsets(const math::vec2& offsets)
{
	if(math::all(math::epsilonEqual(offsets, style_.shadow_offsets, math::epsilon<float>())))
	{
		return;
	}

	style_.shadow_offsets = offsets;
}

void text::set_advance(const math::vec2& advance)
{
	if(math::all(math::epsilonEqual(advance, style_.advance, math::epsilon<float>())))
	{
		return;
	}

	style_.advance = advance;
	clear_lines();
}


void text::set_alignment(align_t a)
{
	if(alignment_ == a)
	{
		return;
	}
	alignment_ = a;
	clear_lines();
}

const std::vector<vertex_2d>& text::get_geometry() const
{
	if(geometry_.empty())
	{
		update_geometry();
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
	update_lines();
	return lines_metrics_;
}

bool text::is_valid() const
{
	return !utf8_text_.empty() && style_.font;
}

float text::get_advance_offset_x() const
{
	auto font = style_.font;
	if(font && font->sdf_spread > 0)
	{
		return style_.advance.x + (style_.outline_width * float(font->sdf_spread + 3) * 2.0f);
	}

	return style_.advance.x;
}
float text::get_advance_offset_y() const
{
	auto font = style_.font;
	if(font && font->sdf_spread > 0)
	{
		return style_.advance.y + (style_.outline_width * float(font->sdf_spread + 3) * 2.0f);
	}

	return style_.advance.y;
}

void text::set_kerning(bool enabled)
{
	if(style_.kerning_enabled == enabled)
	{
		return;
	}
	style_.kerning_enabled = enabled;
	clear_geometry();
}

void text::set_leaning(float leaning)
{
	leaning = math::clamp(leaning, -45.0f, 45.0f);
	if(math::epsilonEqual(leaning, style_.leaning, math::epsilon<float>()))
	{
		return;
	}
	style_.leaning = leaning;
	clear_geometry();
}

void text::set_wrap_width(float max_width)
{
	if(math::epsilonEqual(max_wrap_width_, max_width, math::epsilon<float>()))
	{
		return;
	}
	max_wrap_width_ = max_width;
	clear_lines();
}

void text::set_line_path(const polyline& line)
{
	line_path_ = line;
	clear_lines();
}

void text::set_line_path(polyline&& line)
{
	line_path_ = std::move(line);
	clear_lines();
}

void text::set_decorators(const std::vector<text_decorator>& decorators)
{
	decorators_ = decorators;

	for(auto& dec : decorators_)
	{
		if(dec.unicode_visual_range.empty())
		{
			dec.unicode_visual_range = dec.unicode_range;
		}
	}
	clear_lines();
}
void text::set_decorators(std::vector<text_decorator>&& decorators)
{
	decorators_ = std::move(decorators);

	for(auto& dec : decorators_)
	{
		if(dec.unicode_visual_range.empty())
		{
			dec.unicode_visual_range = dec.unicode_range;
		}
	}
	clear_lines();
}
void text::add_decorator(const text_decorator& decorator)
{
	decorators_.emplace_back(decorator);

	auto& dec = decorators_.back();
	if(dec.unicode_visual_range.empty())
	{
		dec.unicode_visual_range = dec.unicode_range;
	}
	clear_lines();
}
void text::add_decorator(text_decorator&& decorator)
{
	decorators_.emplace_back(decorator);
	auto& dec = decorators_.back();
	if(dec.unicode_visual_range.empty())
	{
		dec.unicode_visual_range = dec.unicode_range;
	}

	clear_lines();
}
const polyline& text::get_line_path() const
{
	return line_path_;
}

const std::string& text::get_utf8_text() const
{
	return utf8_text_;
}

void text::clear_geometry()
{
	geometry_.clear();
}

void text::clear_lines()
{
	chars_ = 0;
	lines_.clear();
	unicode_text_.clear();
    lines_metrics_.clear();
	rect_ = {};

	clear_geometry();
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
	const text_decorator* result = &main_decorator_;

	for(const auto& decorator : decorators_)
	{
		if(current->unicode_range.end >= glyph_idx && current != &decorator && glyph_idx <= decorator.unicode_range.begin)
		{
			if(result == &main_decorator_ || decorator.unicode_range.begin < result->unicode_range.begin)
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

	if(i >= current->unicode_range.end)
	{
		if(current != next && i >= next->unicode_range.begin)
		{
			changed = true;
			current = next;
			next = get_next_decorator(i, current);
		}
		else if(current != &main_decorator_)
		{
			changed = true;
			current = &main_decorator_;
		}
	}
	return changed;
}

float text::get_decorator_scale(const text_decorator* current) const
{
	if(current != &main_decorator_)
	{
		return current->scale;
	}

	return 1.0f;
}

void text::update_lines() const
{
	// if we already generated lines
	if(!lines_.empty())
	{
		return;
	}

	auto font = style_.font;

	if(!font)
	{
		return;
	}

	const auto& unicode_text = get_unicode_text();

	if(unicode_text.empty())
	{
		return;
	}
	// find newlines

	auto last_space = size_t(-1);

	lines_.clear();
	cache<text>::get(lines_, 1);
	lines_.resize(1);

	const auto unicode_text_size = unicode_text.size();
	cache<text>::get(lines_.back(), unicode_text_size);
	lines_.back().reserve(unicode_text_size);

	auto decorator = &main_decorator_;
	auto next_decorator = get_next_decorator(0, decorator);

	auto max_width = float(max_wrap_width_) - font->get_glyph(' ').advance_x * decorator->scale;

    auto advance_offset_x = get_advance_offset_x();
	auto advance_offset_y = get_advance_offset_y();

    float scale = decorator->scale;
	auto line_height = scale * (font->line_height + advance_offset_y);
	const auto ascent = scale * font->ascent;
	const auto descent = scale * font->descent;
	const auto x_height = scale * font->x_height;
	const auto cap_height = scale * font->cap_height;

    auto pen_y = ascent;
    auto pen_x_last_space = 0.0f;

    lines_metrics_.emplace_back();
    auto metric = &lines_metrics_.back();
    metric->minx = 0.0f;
    metric->maxx = 0.0f;
    metric->ascent = pen_y - ascent;
    metric->cap_height = pen_y - cap_height;
    metric->x_height = pen_y - x_height;
    metric->median = pen_y - cap_height * 0.5f;
    metric->baseline = pen_y;
    metric->descent = pen_y - descent;
    metric->miny = metric->ascent;
    metric->maxy = metric->descent;

	for(size_t i = 0; i < unicode_text_size; ++i)
	{
		uint32_t c = unicode_text[i];

		const auto& g = font->get_glyph(c);
		// decorator begin
		{
			get_decorator(chars_, decorator, next_decorator);

			if(decorator->get_size_on_line && decorator->unicode_range.at_end(chars_))
			{
				const char* str_begin = utf8_text_.data() + decorator->utf8_range.begin;
				const char* str_end = utf8_text_.data() + decorator->utf8_range.end;
				auto external_size = decorator->get_size_on_line(*decorator, str_begin, str_end);
                metric->maxx += external_size.first;
            }
		}

		float relative_scale = get_decorator_scale(decorator);
		float glyph_advance = (advance_offset_x + g.advance_x) * scale * relative_scale;

		if(!decorator->is_visible(chars_))
        {
            glyph_advance = 0;
        } 
        // check if is a break possible character
		else if(is_space(c))
		{
			last_space = i;
            pen_x_last_space = metric->maxx;
		}
		// decorator end

		bool exceedsmax_width = max_width > 0 && (metric->maxx + glyph_advance) > max_width;

		if((decorator->is_visible(chars_) && is_newline(c)) || (exceedsmax_width && (last_space != size_t(-1))))
		{
            metric->maxx = pen_x_last_space;

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

            pen_x_last_space = 0;
			last_space = size_t(-1);
            pen_y += line_height;

            lines_metrics_.emplace_back();
            metric = &lines_metrics_.back();
            metric->minx = 0.0f;
            metric->maxx = 0.0f;
            metric->ascent = pen_y - ascent;
            metric->cap_height = pen_y - cap_height;
            metric->x_height = pen_y - x_height;
            metric->median = pen_y - cap_height * 0.5f;
            metric->baseline = pen_y;
            metric->descent = pen_y - descent;
            metric->miny = metric->ascent;
			metric->maxy = metric->descent;

			line_height = scale * (font->line_height + advance_offset_y);
		}
		else
		{
			lines_.back().push_back(c);
			++chars_;
			metric->maxx += glyph_advance;
		}
	}

    update_alignment();
}

void text::update_alignment() const
{
    const auto pixel_snap = style_.font->pixel_snap;

    const auto& first_line = lines_metrics_.front();
    const auto& last_line = lines_metrics_.back();

    float align_y = get_alignment_y(alignment_,
                                    first_line.miny, first_line.baseline, first_line.cap_height,
                                    last_line.maxy, last_line.baseline, last_line.cap_height,
                                    pixel_snap);


    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();

    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();

    for(auto& metric : lines_metrics_)
    {
        metric.miny += align_y;
        metric.maxy += align_y;
        metric.ascent += align_y;
        metric.cap_height += align_y;
        metric.x_height += align_y;
        metric.median += align_y;
        metric.baseline += align_y;
        metric.descent += align_y;

        if(line_path_.empty())
        {
            auto align_x = get_alignment_x(alignment_,
                                           metric.minx,
                                           metric.maxx,
                                           pixel_snap);

            metric.minx += align_x;
            metric.maxx += align_x;
        }

        min_x = std::min(metric.minx, min_x);
        max_x = std::max(metric.maxx, max_x);
        min_y = std::min(metric.miny, min_y);
        max_y = std::max(metric.maxy, max_y);
    }

    rect_.y = min_y;
    rect_.x = min_x;
    rect_.h = max_y - min_y;
    rect_.w = max_x - min_x;
}

void text::update_geometry() const
{
	auto font = style_.font;

	if(!font)
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

	auto decorator = &main_decorator_;
	auto next_decorator = get_next_decorator(0, decorator);
	auto advance_offset_x = get_advance_offset_x();

	const auto color_top = style_.color_top;
	const auto color_bot = style_.color_bot;
	const math::vec4 vcolor_top{color_top.r, color_top.g, color_top.b, color_top.a};
	const math::vec4 vcolor_bot{color_bot.r, color_bot.g, color_bot.b, color_bot.a};
	const bool has_gradient = color_top != color_bot;
	const bool kerning_enabled = style_.kerning_enabled;

	const auto sdf_spread = font->sdf_spread;
	const auto sdf_font = sdf_spread > 0;
    const auto outline_width = style_.outline_width;
    const auto outline_softness = style_.outline_softness;
    const auto outline_color_top = outline_width > 0 ? style_.outline_color_top : color_top;
	const auto outline_color_bot = outline_width > 0 ? style_.outline_color_bot : color_bot;
	const math::vec4 outline_vcolor_top{outline_color_top.r, outline_color_top.g, outline_color_top.b, outline_color_top.a};
	const math::vec4 outline_vcolor_bot{outline_color_bot.r, outline_color_bot.g, outline_color_bot.b, outline_color_bot.a};
	const bool outline_has_gradient = sdf_font && outline_color_top != outline_color_bot;

	float leaning = 0.0f;
	bool has_leaning = false;
	float scale = decorator->scale;
	const auto pixel_snap = font->pixel_snap;
	const auto ascent = scale * font->ascent;
	const auto descent = scale * font->descent;
	const auto height = ascent - descent;

	const auto x_height = scale * font->x_height;
	const auto cap_height = scale * font->cap_height;
	const auto median = cap_height * 0.5f;

    cache<text>::get(geometry_, chars_ * vertices_per_quad);
    geometry_.resize(chars_ * vertices_per_quad);

    has_leaning = math::epsilonNotEqual(style_.leaning, 0.0f, math::epsilon<float>());
    if(has_leaning)
    {
        leaning = math::rotateZ(math::vec3{0.0f, ascent, 0.0f}, math::radians(-style_.leaning)).x;
    }

	auto vptr = geometry_.data();
    size_t vtx_count{};

	size_t i = 0;
	for(size_t line_idx = 0; line_idx < lines.size(); ++line_idx)
	{
        const auto& line = lines[line_idx];
        const auto& metric = lines_metrics_[line_idx];

        // Set glyph positions on a (0,0) baseline.
        // (x0,y0) for a glyph is the bottom-lefts
        auto pen_y = metric.baseline;
        auto pen_x = metric.minx;

		auto last_codepoint = char_t(-1);

		for(auto c : line)
		{
			const auto& g = font->get_glyph(c);
            auto glyph_idx = i++;

			if(is_newline(c))
			{
				continue;
			}

			{
				get_decorator(glyph_idx, decorator, next_decorator);

				if(decorator->unicode_range.at_end(glyph_idx))
				{
					const char* str_begin = utf8_text_.data() + decorator->utf8_range.begin;
					const char* str_end = utf8_text_.data() + decorator->utf8_range.end;

					float external_advance = 0.0f;
					if(decorator->get_size_on_line)
					{
						auto external_size = decorator->get_size_on_line(*decorator, str_begin, str_end);
						external_advance = external_size.first;
					}

                    if(decorator->set_position_on_line)
                    {
                        decorator->set_position_on_line(*decorator, pen_x, line_idx, metric, str_begin, str_end);
                    }

					pen_x += external_advance;
				}
			}

			float relative_scale = get_decorator_scale(decorator);

            auto pen_y_script_line = pen_y;
            switch(decorator->script)
            {
                case script_line::ascent:
                    pen_y_script_line = metric.ascent + ascent * relative_scale;
                break;
                case script_line::cap_height:
                    pen_y_script_line = metric.cap_height + cap_height * relative_scale;
                break;
                case script_line::x_height:
                    pen_y_script_line = metric.x_height + x_height * relative_scale;
                break;
                case script_line::median:
                    pen_y_script_line = metric.median + median * relative_scale;
                break;
                case script_line::baseline:
                    pen_y_script_line = metric.baseline;
                break;
                case script_line::descent:
                    pen_y_script_line = metric.descent + descent * relative_scale;
                break;
                default:
                break;
            }

			if(!decorator->is_visible(glyph_idx))
			{
				continue;
			}

			if(kerning_enabled)
			{
				// modify the pen advance with the kerning from the previous character
				pen_x += font->get_kerning(last_codepoint, g.codepoint) * scale * relative_scale;
				last_codepoint = g.codepoint;
			}

            float leaning0 = 0.0f;
            float leaning1 = 0.0f;

            if(has_leaning)
            {
                const auto y0_offs = g.y0 * scale * relative_scale + ascent;
                const auto y0_factor = 1.0f - y0_offs / ascent;
                leaning0 = leaning * y0_factor;

                const auto y1_offs = g.y1 * scale * relative_scale + ascent;
                const auto y1_factor = 1.0f - y1_offs / ascent;
                leaning1 = leaning * y1_factor;
            }

            auto x0 = pen_x + g.x0 * scale * relative_scale;
            auto x1 = pen_x + g.x1 * scale * relative_scale;
            auto y0 = pen_y_script_line + g.y0 * scale * relative_scale;
            auto y1 = pen_y_script_line + g.y1 * scale * relative_scale;

            if(pixel_snap)
            {
                x0 = float(int(x0));
                x1 = float(int(x1));
            }

            const auto coltop = has_gradient ? get_gradient(vcolor_top, vcolor_bot, y0 - metric.ascent, height) : color_top;
            const auto colbot = has_gradient ? get_gradient(vcolor_top, vcolor_bot, y1 - metric.ascent, height) : color_bot;

            const auto outline_coltop = outline_has_gradient ? get_gradient(outline_vcolor_top, outline_vcolor_bot, y0 - metric.ascent, height) : outline_color_top;
            const auto outline_colbot = outline_has_gradient ? get_gradient(outline_vcolor_top, outline_vcolor_bot, y1 - metric.ascent, height) : outline_color_bot;


            std::array<vertex_2d, 4> quad =
            {{
				{{x0, y0}, {g.u0, g.v0}, coltop, outline_coltop, {outline_width, outline_softness}},
				{{x1, y0}, {g.u1, g.v0}, coltop, outline_coltop, {outline_width, outline_softness}},
				{{x1, y1}, {g.u1, g.v1}, colbot, outline_colbot, {outline_width, outline_softness}},
				{{x0, y1}, {g.u0, g.v1}, colbot, outline_colbot, {outline_width, outline_softness}}
            }};

            if(!apply_line_path(quad, line_path_, x0, pen_x, leaning0, leaning1))
            {
                break;
            }
            std::memcpy(vptr, quad.data(), quad.size() * sizeof(vertex_2d));
            vptr += vertices_per_quad;
            vtx_count += vertices_per_quad;

			pen_x += (advance_offset_x + g.advance_x) * scale * relative_scale;
		}

	}
    geometry_.resize(vtx_count);
}

float text::get_width() const
{
	if(rect_)
	{
		return rect_.w;
	}

	update_lines();

	return rect_.w;
}

float text::get_height() const
{
	if(rect_)
	{
		return rect_.h;
	}

	update_lines();

	return rect_.h;
}

rect text::get_rect() const
{
	return cast_rect(get_frect());
}

const frect& text::get_frect() const
{
    if(rect_)
	{
		return rect_;
	}

	update_lines();

	return rect_;
}

const text_style& text::get_style() const
{
	return style_;
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

		decorator.unicode_range.begin = count_glyphs(text.data(), begin);
		decorator.unicode_range.end = decorator.unicode_range.begin + count_glyphs(begin, end);

		decorator.unicode_visual_range = decorator.unicode_range;

		decorator.utf8_range.begin = size_t(idx);
		decorator.utf8_range.end = decorator.utf8_range.begin + size_t(sz);

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

			decorator.unicode_visual_range.begin = count_glyphs(text.data(), local_begin);
			decorator.unicode_visual_range.end = decorator.unicode_visual_range.begin + count_glyphs(local_begin, local_end);

			decorator.utf8_range.begin = size_t(idx + local_idx);
			decorator.utf8_range.end = decorator.utf8_range.begin + size_t(local_sz);


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

					decorator.unicode_visual_range.begin = count_glyphs(text.data(), local_begin);
					decorator.unicode_visual_range.end = decorator.unicode_visual_range.begin + count_glyphs(local_begin, local_end);

					decorator.utf8_range.begin = size_t(idx + local_idx);
					decorator.utf8_range.end = decorator.utf8_range.begin + size_t(local_sz);
				}
			}

		}

	}

	if(!decorators_.empty())
	{
		auto& decorator = decorators_.back();
		if(!decorator.unicode_range.end)
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

	clear_lines();

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
    return main_decorator_;
}

bool text_decorator::is_visible(size_t idx) const
{
    return unicode_visual_range.contains(idx) && !get_size_on_line && !set_position_on_line;

}

}
