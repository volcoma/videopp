#include "text.h"
#include "font.h"
#include "texture.h"

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
// ideographic space (0x3000, ' ')
// are classified as blank characters.
bool is_blank(uint32_t c)
{
    return c == 0x20 || c == 0x09 || c == 0x3000;
}

bool is_punctuation(uint32_t c)
{
    return
//            c == '.' ||
//            c == ',' ||
//            c == ';' ||
//            c == '!' ||
//            c == '?' ||
//            c == '\"' ||
            c == 0x3001 ||
            c == 0x3002 ||
            c == 0xFF0C ||
            c == 0xFF01 ||
            c == 0xFF1F;
}

bool is_white_space(uint32_t c)
{
    return is_blank(c) || c == 0x0b || c == 0x0c || c == 0x0d;
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

// Distances from the baseline to the given metric
struct line_metrics_distances : private line_metrics
{
    using line_metrics::ascent;
    using line_metrics::cap_height;
    using line_metrics::x_height;
    using line_metrics::median;
    using line_metrics::descent;
    using line_metrics::miny;
    using line_metrics::maxy;
};

line_metrics_distances get_max_line_metrics_distances(const line_metrics_distances& metrics_distances, const line_metrics& metrics)
{
    line_metrics_distances max_metrics_distances;
    max_metrics_distances.miny = std::max(metrics_distances.miny, metrics.baseline - metrics.miny);
    max_metrics_distances.ascent = std::max(metrics_distances.ascent, metrics.baseline - metrics.ascent);
    max_metrics_distances.cap_height = std::max(metrics_distances.cap_height, metrics.baseline - metrics.cap_height);
    max_metrics_distances.x_height = std::max(metrics_distances.x_height, metrics.baseline - metrics.x_height);
    max_metrics_distances.median = std::max(metrics_distances.median, metrics.baseline - metrics.median);
    max_metrics_distances.descent = std::max(metrics_distances.descent, metrics.descent - metrics.baseline);
    max_metrics_distances.maxy = std::max(metrics_distances.maxy, metrics.maxy - metrics.baseline);
    return max_metrics_distances;
}

void adjust_line_height_metrics(const line_metrics_distances& metrics_distances, line_metrics& metrics)
{
    metrics.miny = metrics.baseline - metrics_distances.miny;
    metrics.ascent = metrics.baseline - metrics_distances.ascent;
    metrics.cap_height = metrics.baseline - metrics_distances.cap_height;
    metrics.x_height = metrics.baseline - metrics_distances.x_height;
    metrics.median = metrics.baseline - metrics_distances.median;
    metrics.descent = metrics.baseline + metrics_distances.descent;
    metrics.maxy = metrics.baseline + metrics_distances.maxy;
}

// Expands the @metrics distance between it's metrics if the corresponding distances in @metrics_distances are bigger.
void adjust_to_maximal_metrics(const line_metrics_distances& metrics_distances, line_metrics& metrics)
{
    line_metrics adjusted_metric;
    adjusted_metric.baseline = metrics.baseline;

    const auto dist_from_baseline_to_miny = metrics.baseline - metrics.miny;
    if(metrics_distances.miny > dist_from_baseline_to_miny)
    {
        // Push the baseline down.
        // Note that metrics_distances.miny is the maximal posible distance above the baseline.
        adjusted_metric.baseline += metrics_distances.miny - dist_from_baseline_to_miny;
    }

    const auto max_metrics_distances = get_max_line_metrics_distances(metrics_distances, metrics);
    adjust_line_height_metrics(max_metrics_distances, adjusted_metric);

    // Do not make any adjusments over X coord.
    adjusted_metric.minx = metrics.minx;
    adjusted_metric.maxx = metrics.maxx;

    metrics = adjusted_metric;
}

void set_default_line_metric(line_metrics* metric, float baseline, const line_metrics& font_metric)
{
    metric->minx = 0.0f;
    metric->maxx = 0.0f;
    metric->ascent = baseline - font_metric.ascent;
    metric->cap_height = baseline - font_metric.cap_height;
    metric->x_height = baseline - font_metric.x_height;
    metric->median = baseline - font_metric.median;
    metric->baseline = baseline;
    metric->descent = baseline - font_metric.descent;
    metric->miny = metric->ascent;
    metric->maxy = metric->descent;
}

frect apply_typography_adjustment(const text::bounds_query& query,
                                  const frect& r,
                                  align_t align,
                                  const std::vector<line_metrics>& metrics)
{
    std::pair<float, float> adjustment{};

    switch (query)
    {
        case text::bounds_query::precise:
        {
            if(!metrics.empty())
            {
                const auto& first_line = metrics.front();
                const auto& last_line = metrics.back();

                if(align & align::typographic_mask)
                {
                    adjustment.first  = first_line.miny - first_line.cap_height;
                    adjustment.second = last_line.maxy - last_line.baseline;
                }
            }
        }
        break;
        default:
        break;
    }

    auto result = r;
    result.y -= adjustment.first;
    result.h -= adjustment.second - adjustment.first;
    return result;
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

    if(alignment & align::median)
    {
        yoffs = (-miny_cap -maxy_baseline) / 2.0f;
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
    set_font(style.font);
    set_advance(style.advance);
    set_vgradient_colors(style.color_top, style.color_bot);
    set_outline_vgradient_colors(style.outline_color_top, style.outline_color_bot);
    set_shadow_offsets(style.shadow_offsets);
    set_shadow_vgradient_colors(style.shadow_color_top, style.shadow_color_bot);
    set_shadow_softness(style.shadow_softness);
    set_softness(style.softness);
    set_outline_width(style.outline_width);
    set_scale(style.scale);
    set_leaning(style.leaning);
    set_outline_advance(style.outline_advance);
    set_kerning(style.kerning_enabled);
}

void text::set_scale(float scale)
{
    if(math::epsilonEqual(style_.scale, scale, math::epsilon<float>()))
    {
        return;
    }

    style_.scale = scale;
    main_decorator_.scale = scale;
    clear_lines();
}

void text::set_font(const font_ptr& f, int sz_override)
{
    bool changed = false;

    if(sz_override != -1 && f)
    {
        float calculated_scale = float(sz_override)/ float(f->size);
        if(math::epsilonNotEqual(style_.scale, calculated_scale, math::epsilon<float>()))
        {
            changed = true;
            style_.scale = calculated_scale;
            main_decorator_.scale = calculated_scale;
        }
    }

    if(style_.font == f && !changed)
    {
        return;
    }
    style_.font = f;
    clear_lines();
}

align_t text::get_alignment() const noexcept
{
    return alignment_;
}

void text::set_opacity(float opacity)
{
    opacity = math::clamp(opacity, 0.0f, 1.0f);
    if(math::epsilonEqual(opacity_, opacity, math::epsilon<float>()))
    {
        return;
    }
    opacity_ = opacity;
    clear_lines();
}

float text::get_opacity() const noexcept
{
    return opacity_;
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
    owidth = math::clamp(owidth, 0.0f, 1.0f);
    if(math::epsilonEqual(style_.outline_width, owidth, math::epsilon<float>()))
    {
        return;
    }
    style_.outline_width = owidth;
    clear_lines();
}

void text::set_outline_advance(bool oadvance)
{
    if(style_.outline_advance == oadvance)
    {
        return;
    }
    style_.outline_advance = oadvance;
    clear_lines();
}

void text::set_softness(float softness)
{
    softness = math::clamp(softness, 0.0f, 1.0f);
    if(math::epsilonEqual(style_.softness, softness, math::epsilon<float>()))
    {
        return;
    }
    style_.softness = softness;
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

void text::set_shadow_softness(float softness)
{
    softness = math::clamp(softness, 0.0f, 1.0f);
    if(math::epsilonEqual(style_.shadow_softness, softness, math::epsilon<float>()))
    {
        return;
    }
    style_.shadow_softness = softness;
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

void text::set_alignment(align_t flag)
{
    if(alignment_ == flag)
    {
        return;
    }
    alignment_ = flag;
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
    if(style_.outline_advance && font && font->sdf_spread > 0)
    {
        return style_.advance.x + (style_.outline_width * float(font->sdf_spread));
    }

    return style_.advance.x;
}
float text::get_advance_offset_y() const
{
    auto font = style_.font;
    if(style_.outline_advance && font && font->sdf_spread > 0)
    {
        return style_.advance.y + (style_.outline_width * float(font->sdf_spread));
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

void text::set_overflow_type(overflow_type overflow)
{
    if(overflow_ == overflow)
    {
        return;
    }
    overflow_ = overflow;
    clear_lines();
}

void text::set_line_height_behaviour(line_height_behaviour behaviour)
{
    if(line_height_behaviour_ == behaviour)
    {
        return;
    }

    line_height_behaviour_ = behaviour;
    clear_lines();
}

overflow_type text::get_overflow_type() const
{
    return overflow_;
}

auto text::get_line_height_behaviour() const -> line_height_behaviour
{
    return line_height_behaviour_;
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

color text::apply_opacity(color c) const noexcept
{
    c.a = uint8_t(float(c.a) * opacity_);
    return c;
}

const text_decorator* text::get_next_decorator(size_t glyph_idx, const text_decorator* current) const
{
    const text_decorator* result = &main_decorator_;

    bool current_is_main = current == result;
    bool check = current_is_main || current->unicode_range.end >= glyph_idx;

    if(check)
    {
        for(const auto& decorator : decorators_)
        {
            if(current != &decorator && glyph_idx <= decorator.unicode_range.begin)
            {
                if(result == &main_decorator_ || decorator.unicode_range.begin < result->unicode_range.begin)
                {
                    result = &decorator;
                }
            }
        }
    }

    return result;
}

bool text::get_decorator(size_t i, const text_decorator*& current, const text_decorator*& next) const
{
    bool changed = false;

//  simple but slow
//
//    for(const auto& dec : decorators_)
//    {
//        if(dec.unicode_range.contains(i))
//        {
//            current = &dec;
//            changed = true;
//            break;
//        }
//    }
//    if(!changed)
//    {
//        current = &main_decorator_;
//    }


    // ugly but fast, this is a hotpath for every char
    if(i >= current->unicode_range.end)
    {
        if(current != next && i >= next->unicode_range.begin)
        {
            current = next;
            next = get_next_decorator(i, current);
            changed = current != next;
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

    struct line_breaker
    {
        enum class type
        {
            none,
            space,
            punctuation_space,
            decorator_end,
            space_sequence,
        };

        size_t pos{static_cast<size_t>(-1)};
        type t{type::none};
        float pen_x{0.0f};
        float extra_advance_x{0.0f};
        float accumulated_glyph_advance{0.0f};

        line_metrics_distances max_dec_metrics_distances;

        bool is_valid() const { return t != type::none; }

        void reset(type br_type, size_t p, float x, float extra_advance, const line_metrics_distances& max_decorator_metrics, float accumulatable_x = 0.0f)
        {
            accumulated_glyph_advance = 0.0f;

            t = br_type;
            pen_x = x;
            pos = p;
            extra_advance_x = extra_advance;
            accumulated_glyph_advance += accumulatable_x;
            max_dec_metrics_distances = max_decorator_metrics;
        }

        void reset()
        {
            pos = static_cast<size_t>(-1);
            t = type::none;
            pen_x = 0.0f;
            extra_advance_x = 0.0f;
            accumulated_glyph_advance = 0.0f;
            max_dec_metrics_distances = line_metrics_distances();
        }

        void set(type br_type, size_t p, float x, float extra_advance, const line_metrics_distances& max_decorator_metrics, float accumulatable_x = 0.0f)
        {
            t = br_type;
            pen_x = x;
            pos = p;
            extra_advance_x = extra_advance;
            accumulated_glyph_advance += accumulatable_x;
            max_dec_metrics_distances = max_decorator_metrics;
        }
    };

    const auto is_splittable = (overflow_ == overflow_type::word_break || overflow_ == overflow_type::word) && max_wrap_width_ > 0.0f;
    constexpr int max_iterations = 2;
    for(int iteration = 0; iteration < max_iterations; ++iteration)
    {
        line_breaker last_line_breaker;
        bool is_prev_symbol_space{};

        chars_ = 0;

        lines_.clear();
        cache<text>::get(lines_, 1);
        lines_.resize(1);

        const auto unicode_text_size = unicode_text.size();
        cache<text>::get(lines_.back(), unicode_text_size);
        lines_.back().reserve(unicode_text_size);

        auto decorator = &main_decorator_;
        auto next_decorator = get_next_decorator(0, decorator);

        const auto space_advance = font->get_glyph(' ').advance_x * decorator->scale;
        auto max_width = float(max_wrap_width_) - space_advance;

        auto advance_offset_x = get_advance_offset_x();
        auto advance_offset_y = get_advance_offset_y();

        const auto kerning_enabled = style_.kerning_enabled;
        const auto scale = decorator->scale * get_small_caps_scale();
        const auto line_padding = scale * advance_offset_y;
        const auto line_height = scale * font->line_height + line_padding;
        line_metrics font_metric;
        font_metric.ascent = scale * font->ascent;
        font_metric.descent = scale * font->descent;
        font_metric.x_height = scale * font->x_height;
        font_metric.cap_height = scale * font->cap_height;
        font_metric.median = font_metric.cap_height * 0.5f;

        lines_metrics_.emplace_back();
        // Note: @metric is in absolute increasing values: miny <= ... <= baseline <= ... <= maxy.
        auto metric = &lines_metrics_.back();
        set_default_line_metric(metric, font_metric.ascent, font_metric);

        auto last_codepoint = char_t(-1);

        // Keeps the maximal decorator distances between the baseline and the other metric lines.
        line_metrics_distances max_dec_metrics_distances;

        // Helper functions.
        auto add_char = [&](uint32_t c, float advance)
        {
            lines_.back().push_back(c);
            ++chars_;
            metric->maxx += advance;
        };
        auto split_line = [&]()
        {
            last_line_breaker.reset();
            is_prev_symbol_space = false;

            auto diff = unicode_text_size - chars_;

            lines_.resize(lines_.size() + 1);
            cache<text>::get(lines_.back(), diff);
            lines_.back().reserve(diff);

            if(line_height_behaviour_ == line_height_behaviour::dynamic)
            {
                adjust_to_maximal_metrics(max_dec_metrics_distances, *metric);
                max_dec_metrics_distances = line_metrics_distances();
            }

            const auto height_bellow_baseline = metric->maxy - metric->baseline;
            const auto next_line_baseline =
                metric->baseline +
                std::max(line_height, height_bellow_baseline + font_metric.ascent + line_padding);

            lines_metrics_.emplace_back();
            metric = &lines_metrics_.back();
            set_default_line_metric(metric, next_line_baseline, font_metric);
        };
        auto break_line = [&](size_t& i)
        {
            assert(last_line_breaker.is_valid() && "Expected valid line breaker!");

            metric->maxx = last_line_breaker.pen_x - last_line_breaker.accumulated_glyph_advance - last_line_breaker.extra_advance_x;

            auto diff = uint32_t(i - (last_line_breaker.pos + 1));
            lines_.back().resize(lines_.back().size() - diff);
            chars_ -= diff;
            i = last_line_breaker.pos;

            // adjust decorators
            decorator = &main_decorator_;
            for(const auto& dec : decorators_)
            {
                if(dec.unicode_range.contains(chars_))
                {
                    decorator = &dec;
                    break;
                }
            }
            next_decorator = get_next_decorator(chars_, decorator);

            if(line_height_behaviour_ == line_height_behaviour::dynamic)
            {
                max_dec_metrics_distances = last_line_breaker.max_dec_metrics_distances;
            }

            split_line();
        };
        auto update_max_dec_metrics = [&](const line_metrics& first_line_dec_metrics, float dec_height)
        {
#ifdef MAX_DEC_PROP
#error "Change the macro name"
#endif
#define MAX_DEC_PROP(prop)                                                                        \
            max_dec_metrics_distances.prop =                                                      \
                std::max(max_dec_metrics_distances.prop,                                          \
                        std::abs(first_line_dec_metrics.prop - first_line_dec_metrics.baseline))

            MAX_DEC_PROP(miny);
            MAX_DEC_PROP(ascent);
            MAX_DEC_PROP(cap_height);
            MAX_DEC_PROP(x_height);
            MAX_DEC_PROP(median);
            MAX_DEC_PROP(descent);
#undef MAX_DEC_PROP

            max_dec_metrics_distances.maxy = std::max(max_dec_metrics_distances.maxy,
                                                      dec_height - std::abs(first_line_dec_metrics.miny - first_line_dec_metrics.baseline));
        };
        auto update_fixed_line_heights = [&]()
        {
            // All lines should have the same height - the height of the tallest line.

            assert(line_height_behaviour_ == line_height_behaviour::fixed);
            assert(!lines_metrics_.empty() && "Expected non empty line metrics.");
            const auto& first_line_metrics = lines_metrics_.front();

            const auto has_dec_bigger_metric_distance =
                    max_dec_metrics_distances.miny > first_line_metrics.baseline - first_line_metrics.miny ||
                    max_dec_metrics_distances.ascent > first_line_metrics.baseline - first_line_metrics.ascent ||
                    max_dec_metrics_distances.cap_height > first_line_metrics.baseline - first_line_metrics.cap_height ||
                    max_dec_metrics_distances.x_height > first_line_metrics.baseline - first_line_metrics.x_height ||
                    max_dec_metrics_distances.median > first_line_metrics.baseline - first_line_metrics.median ||
                    max_dec_metrics_distances.descent > first_line_metrics.descent - first_line_metrics.baseline ||
                    max_dec_metrics_distances.maxy > first_line_metrics.maxy - first_line_metrics.baseline;
            if(!has_dec_bigger_metric_distance)
            {
                return;
            }

            const auto new_line_metrics_distances = get_max_line_metrics_distances(max_dec_metrics_distances, first_line_metrics);
            const auto new_line_height = new_line_metrics_distances.maxy + new_line_metrics_distances.miny;
            const auto new_line_total_height = std::max(line_height, new_line_height + line_padding);
            auto baseline = new_line_metrics_distances.miny;

            for(auto& line_metrics : lines_metrics_)
            {
                line_metrics.baseline = baseline;
                adjust_line_height_metrics(new_line_metrics_distances, line_metrics);

                baseline += new_line_total_height;
            }
        };

        for(size_t i = 0; i < unicode_text_size; ++i)
        {
            const auto is_last_symbol = i == unicode_text_size - 1;
            const auto is_beginning_of_line = lines_.back().empty();
            const auto c = unicode_text[i];
            const auto& g = font->get_glyph(c);
            // Decorator handling.
            get_decorator(chars_, decorator, next_decorator);

            // Get relative scale from decorator
            const auto relative_scale = get_decorator_scale(decorator);
            const auto extra_advance = advance_offset_x * scale * relative_scale;

            // Calculate unscaled glyph advance
            auto glyph_advance = (g.advance_x + advance_offset_x);

            if(kerning_enabled)
            {
                // Modify the pen advance with the kerning from the previous character.
                glyph_advance += font->get_kerning(last_codepoint, g.codepoint);
                last_codepoint = g.codepoint;
            }
            glyph_advance *= scale * relative_scale;

            // Note: we will treat the whitespace(s)(all kinds white spaces)
            // as not visual characers when they are at the end of the line.

            const auto is_start_of_external_decorator = decorator->get_size_on_line && decorator->unicode_range.begin == chars_;
            if(is_start_of_external_decorator)
            {
                // The decorator interrupts the kerning.
                last_codepoint = char_t(-1);

                const char* str_begin = utf8_text_.data() + decorator->utf8_visual_range.begin;
                const char* str_end = utf8_text_.data() + decorator->utf8_visual_range.end;
                auto external_size = decorator->get_size_on_line(*decorator, *metric, str_begin, str_end);

                const auto is_max_width_reached = max_width > 0 && (metric->maxx + external_size.width) > max_width;
                if(is_max_width_reached)
                {
                    if(is_splittable && last_line_breaker.is_valid())
                    {
                        break_line(i);
                        continue;
                    }

                    if(!is_beginning_of_line && overflow_ == overflow_type::word_break)
                    {
                        // Process the decorator on a new line in the next iteration.
                        split_line();
                        --i;
                        continue;
                    }
                }

                // Add all characters in the decorator to the line.
                while(chars_ < decorator->unicode_range.end)
                {
                    add_char(unicode_text_[i], 0.0f);
                    ++i;
                }
                --i; // Reposition @i to point the last decorator's character.

                metric->maxx += external_size.width;

                update_max_dec_metrics(external_size.first_line_metrics, external_size.height);

                is_prev_symbol_space = false;
                continue;
            }

            if(!decorator->is_visible(chars_))
            {
                add_char(c, 0.0f);
                continue;
            }

            const auto is_max_width_reached = max_width > 0 && (metric->maxx + glyph_advance) > max_width;

            // Directly split the line when there is a newline character.
            if(is_newline(c))
            {
                if(is_splittable)
                {
                    // Remove the whitespace(s) glypth advance
                    if(last_line_breaker.is_valid() && is_prev_symbol_space)
                    {
                        metric->maxx -= last_line_breaker.accumulated_glyph_advance;
                    }
                }
                glyph_advance = 0.0f; // To not advance it with newline width
                add_char(c, glyph_advance);
                split_line();
                continue;
            }

            if(is_last_symbol)
            {
                glyph_advance -= extra_advance;
            }

            // Do not make any modifications if the overflow is not splittable, i.e. leave the text as is.
            if(!is_splittable)
            {
                add_char(c, glyph_advance);
                continue;
            }

            if(is_last_symbol && is_white_space(c)) // Text ends with whitespace(s)
            {
                // Remove the whitespace(s) glyph advancing at the end of the text.
                if(last_line_breaker.is_valid() && is_prev_symbol_space)
                {
                    metric->maxx -= last_line_breaker.accumulated_glyph_advance;
                }
                add_char(c, 0.0f);
                continue;
            }

            if(is_white_space(c))
            {
                add_char(c, glyph_advance);

                if(!is_prev_symbol_space)
                {
                    last_line_breaker.reset();
                }

                const auto type =
                        is_prev_symbol_space ? line_breaker::type::space_sequence : line_breaker::type::space;
                last_line_breaker.set(type, i, metric->maxx, extra_advance, max_dec_metrics_distances, glyph_advance);

                is_prev_symbol_space = true;
                continue;
            }
            is_prev_symbol_space = false;

            if(is_max_width_reached)
            {
                if(is_beginning_of_line)
                {
                    add_char(c, glyph_advance);
                    continue;
                }

                switch(overflow_)
                {
                    case overflow_type::word_break:
                    {
                        if(!last_line_breaker.is_valid())
                        {
                            split_line();
                            --i; // Process @c in the next iteration.
                            continue;
                        }

                        break_line(i);
                        continue;
                    }
                    case overflow_type::word:
                    {
                        if(last_line_breaker.is_valid())
                        {
                            break_line(i);
                            continue;
                        }
                        add_char(c, glyph_advance);
                    }
                    break;
                    default:
                    {
                        add_char(c, glyph_advance);
                    }
                    break;
                }
            }
            else
            {
                add_char(c, glyph_advance);
            }

            if(is_punctuation(c))
            {
                last_line_breaker.reset(line_breaker::type::punctuation_space, i, metric->maxx, extra_advance, max_dec_metrics_distances, 0.0f);
            }
        }

        if(line_height_behaviour_ == line_height_behaviour::dynamic)
        {
            // Update last line's metrics.
            adjust_to_maximal_metrics(max_dec_metrics_distances, *metric);
        }
        else if(line_height_behaviour_ == line_height_behaviour::fixed)
        {
            update_fixed_line_heights();
        }

        update_alignment();

        if(overflow_ == overflow_type::none)
        {
            break;
        }

        // If there was a line exceeding max_wrap_width
        // which could not be broken. Thus it should extend
        // the wrap with to its width.
        // If we don't do this previous lines would've calculated
        // based on a the user passed wrap width, and not calculated
        // longer one.
        //
        // Consider this example using the overflow's word value:
        // As we are iterating line by line from top to bottom

        // Original             \ Wrongly wrapped
        //------------------------------------------------------
        // some text            \ some text
        // some more text       \ some more
        // someunbreakabletext  \ text
        //         ^            \ someunbreakabletext
        //                                ^
        // wrap width

        // This would result in the example above which is NOT correct
        // so we need to extend the user provided width and calculate again

        // some text
        // some more text
        // someunbreakabletext
        //         ^    ->   ^

        const auto is_last_iteration = iteration >= max_iterations - 1;
        if(!is_last_iteration && max_width > 0 && rect_.w > (max_width + 1.0f))
        {
            max_wrap_width_ = rect_.w + space_advance;
            chars_ = 0;
            lines_.clear();
            lines_metrics_.clear();
            rect_ = {};
        }
        else
        {
            break;
        }
    }
}

void text::update_alignment() const
{
    if(lines_metrics_.empty())
    {
        return;
    }
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
    float scale = decorator->scale * get_small_caps_scale();

    auto advance_offset_x = get_advance_offset_x();

    const auto color_top = apply_opacity(style_.color_top);
    const auto color_bot = apply_opacity(style_.color_bot);
    const math::vec4 vcolor_top{color_top.r, color_top.g, color_top.b, color_top.a};
    const math::vec4 vcolor_bot{color_bot.r, color_bot.g, color_bot.b, color_bot.a};
    const bool has_gradient = color_top != color_bot;
    const bool kerning_enabled = style_.kerning_enabled;

    auto sdf_spread = font->sdf_spread * scale;
    const auto sdf_font = sdf_spread > 0;
    const auto outline_width = style_.outline_width;
    const auto softness = style_.softness;
    const auto outline_color_top = outline_width > 0 ? apply_opacity(style_.outline_color_top) : color_top;
    const auto outline_color_bot = outline_width > 0 ? apply_opacity(style_.outline_color_bot) : color_bot;

    const math::vec4 outline_vcolor_top{outline_color_top.r, outline_color_top.g, outline_color_top.b, outline_color_top.a};
    const math::vec4 outline_vcolor_bot{outline_color_bot.r, outline_color_bot.g, outline_color_bot.b, outline_color_bot.a};
    const bool outline_has_gradient = sdf_font && outline_color_top != outline_color_bot;

    float leaning = 0.0f;
    bool has_leaning = false;
    const auto pixel_snap = font->pixel_snap;
    const auto ascent = scale * font->ascent;
    const auto descent = scale * font->descent;
    const auto height = ascent - descent;

    const auto x_height = scale * font->x_height;
    const auto cap_height = scale * font->cap_height;
    const auto median = cap_height * 0.5f;

    auto scaled_spread = font->sdf_spread * (style_.softness > 0.0f ? 1.0f : std::max(0.1f, style_.outline_width));
    auto sdf_shift_x = fnt::calc_shift(scaled_spread, font->texture->get_rect().w);
    auto sdf_shift_y = fnt::calc_shift(scaled_spread, font->texture->get_rect().h);

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
            const auto& gl = font->get_glyph(c);
            auto g = fnt::shift(gl, sdf_shift_x, sdf_shift_y);
            auto glyph_idx = i++;

            if(is_newline(c))
            {
                continue;
            }

            auto pen_y_decorated = pen_y;
            {
                get_decorator(glyph_idx, decorator, next_decorator);

                if(decorator->unicode_range.at_end(glyph_idx))
                {
                    const char* str_begin = utf8_text_.data() + decorator->utf8_visual_range.begin;
                    const char* str_end = utf8_text_.data() + decorator->utf8_visual_range.end;

                    float external_advance = 0.0f;
                    if(decorator->get_size_on_line)
                    {
                        auto external_size = decorator->get_size_on_line(*decorator, metric, str_begin, str_end);
                        external_advance = external_size.width;
                    }

                    if(decorator->set_position_on_line)
                    {
                        decorator->set_position_on_line(*decorator, pen_x, line_idx, metric, str_begin, str_end);
                    }

                    pen_x += external_advance;
                }
            }

            float relative_scale = get_decorator_scale(decorator);
            line_metrics metric_relative_to_font = metric;
            metric_relative_to_font.ascent = metric.baseline - ascent;
            metric_relative_to_font.cap_height = metric.baseline - cap_height;
            metric_relative_to_font.x_height = metric.baseline - x_height;
            metric_relative_to_font.median = metric.baseline - median;
            metric_relative_to_font.descent = metric.baseline - descent;

            switch(decorator->script)
            {
                case script_line::ascent:
                    pen_y_decorated = metric_relative_to_font.ascent + ascent * relative_scale;
                    break;
                case script_line::cap_height:
                    pen_y_decorated = metric_relative_to_font.cap_height + cap_height * relative_scale;
                    break;
                case script_line::x_height:
                    pen_y_decorated = metric_relative_to_font.x_height + x_height * relative_scale;
                    break;
                case script_line::median:
                    pen_y_decorated = metric_relative_to_font.median + median * relative_scale;
                    break;
                case script_line::baseline:
                    pen_y_decorated = metric_relative_to_font.baseline;
                    break;
                case script_line::descent:
                    pen_y_decorated = metric_relative_to_font.descent + descent * relative_scale;
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
            auto y0 = pen_y_decorated + g.y0 * scale * relative_scale;
            auto y1 = pen_y_decorated + g.y1 * scale * relative_scale;

            if(pixel_snap)
            {
                x0 = float(int(x0));
                x1 = float(int(x1));
            }

            auto y0_offset = y0 - pen_y_decorated + ascent;
            auto y1_offset = y1 - pen_y_decorated + ascent;

            auto coltop = has_gradient ? get_gradient(vcolor_top, vcolor_bot, y0_offset, height) : color_top;
            auto colbot = has_gradient ? get_gradient(vcolor_top, vcolor_bot, y1_offset, height) : color_bot;

            auto outline_coltop = outline_has_gradient ? get_gradient(outline_vcolor_top, outline_vcolor_bot, y0_offset, height) : outline_color_top;
            auto outline_colbot = outline_has_gradient ? get_gradient(outline_vcolor_top, outline_vcolor_bot, y1_offset, height) : outline_color_bot;

            std::array<vertex_2d, 4> quad =
            {{
                {{x0, y0}, {g.u0, g.v0}, coltop, outline_coltop, {outline_width, softness}},
                {{x1, y0}, {g.u1, g.v0}, coltop, outline_coltop, {outline_width, softness}},
                {{x1, y1}, {g.u1, g.v1}, colbot, outline_colbot, {outline_width, softness}},
                {{x0, y1}, {g.u0, g.v1}, colbot, outline_colbot, {outline_width, softness}}
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

float text::get_wrap_width() const
{
    return max_wrap_width_;
}

rect text::get_rect() const
{
    return cast_rect(get_bounds());
}

frect text::get_bounds(bounds_query query) const
{
    if(rect_)
    {
        return apply_typography_adjustment(query, rect_, alignment_, lines_metrics_);
    }

    update_lines();

    return apply_typography_adjustment(query, rect_, alignment_, lines_metrics_);
}

const text_style& text::get_style() const
{
    return style_;
}

std::vector<text_decorator*> text::add_decorators(const std::string& style_id)
{
//    const std::string braces_matcher = R"(\[([\s\S]*?)\])";
//    const std::string match_style = style_id + braces_matcher;

//    const std::regex global_rx(match_style); // --> style_id[some text]
//    const std::regex local_rx(braces_matcher); // --> [some text]

//    return add_decorators(global_rx, local_rx);

    return add_decorators(style_id + "[", "]");
}

std::vector<text_decorator*> text::add_decorators(const std::string& start_str,
                                                  const std::string& end_str)
{
    auto sz_before = decorators_.size();

    const auto& text = get_utf8_text();

    std::string::size_type start_offset = 0;
    while(true)
    {
        auto prefix_pos = text.find(start_str, start_offset);
        if(prefix_pos == std::string::npos)
        {
            break;
        }

        auto postfix_pos = text.find(end_str, prefix_pos + start_str.size());
        if(postfix_pos == std::string::npos)
        {
            break;
        }
        decorators_.emplace_back(get_decorator());
        auto& decorator = decorators_.back();

        auto begin = text.data() + prefix_pos;
        auto end = text.data() + postfix_pos + end_str.size();

        decorator.unicode_range.begin = text::count_glyphs(text.data(), begin);
        decorator.unicode_range.end = decorator.unicode_range.begin + text::count_glyphs(begin, end);


        decorator.utf8_visual_range.begin = prefix_pos + start_str.size();
        decorator.utf8_visual_range.end = postfix_pos;

        auto local_begin = begin + start_str.size();
        auto local_end = end - end_str.size();

        decorator.unicode_visual_range.begin = text::count_glyphs(text.data(), local_begin);
        decorator.unicode_visual_range.end = decorator.unicode_visual_range.begin + text::count_glyphs(local_begin, local_end);

        start_offset = postfix_pos;

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

    clear_geometry();

    return result;
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

        decorator.utf8_visual_range.begin = size_t(idx);
        decorator.utf8_visual_range.end = decorator.utf8_visual_range.begin + size_t(sz);

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

            decorator.utf8_visual_range.begin = size_t(idx + local_idx);
            decorator.utf8_visual_range.end = decorator.utf8_visual_range.begin + size_t(local_sz);


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

                    decorator.utf8_visual_range.begin = size_t(idx + local_idx);
                    decorator.utf8_visual_range.end = decorator.utf8_visual_range.begin + size_t(local_sz);
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

void text::clear_decorators_with_callbacks()
{
    auto end = std::remove_if(std::begin(decorators_), std::end(decorators_),
                              [](const auto& decorator)
                              {
                                  return decorator.get_size_on_line || decorator.set_position_on_line;
                              });

    decorators_.erase(end, std::end(decorators_));

    clear_lines();
}

bool text::get_small_caps() const
{
    return small_caps_;
}

float text::get_small_caps_scale() const
{
    return get_small_caps() ? 0.75f : 1.0f;
}

float text::get_line_height() const
{
    const auto scale = style_.scale * get_small_caps_scale();
    if(style_.font)
    {
        return style_.font->line_height * scale;
    }

    return 1.0f;
}

void text::set_small_caps(bool small_caps)
{
    if(small_caps_ == small_caps)
    {
        return;
    }
    small_caps_ = small_caps;
    clear_lines();
}

const text_decorator& text::get_decorator() const
{
    return main_decorator_;
}

bool text_decorator::is_visible(size_t idx) const
{
    return unicode_visual_range.contains(idx) && !get_size_on_line && !set_position_on_line;

}

std::string to_string(overflow_type overflow)
{
    switch(overflow)
    {
    case overflow_type::word:
        return "word";

    case overflow_type::word_break:
        return "word_break";

    default:
        return "none";
    }
}

std::string to_string(text::line_height_behaviour behaviour)
{
    switch(behaviour)
    {
    case text::line_height_behaviour::fixed:
        return "fixed";

    case text::line_height_behaviour::dynamic:
        return "dynamic";

    default:
        assert(false && "Not implemented!");
        return "fixed";
    }
}

template<>
text::line_height_behaviour from_string<text::line_height_behaviour>(const std::string& str)
{
#define switch_case(val, val2) if(str == (val)) return val2

    switch_case("fixed", text::line_height_behaviour::fixed);
    switch_case("dynamic", text::line_height_behaviour::dynamic);

    return text::line_height_behaviour::fixed;
}

template<>
overflow_type from_string<overflow_type>(const std::string& str)
{
#define switch_case(val, val2) if(str == (val)) return val2

    switch_case("word", overflow_type::word);
    switch_case("word_break", overflow_type::word_break);
    switch_case("none", overflow_type::none);

    return overflow_type::word_break;
}

}
