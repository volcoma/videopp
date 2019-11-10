#include "text.h"
#include "font.h"
#include <algorithm>
#include <iostream>

namespace gfx
{


namespace
{

constexpr size_t vertices_per_quad = 4;

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
                      float miny, float miny_baseline,
                      float maxy, float maxy_baseline, bool /*pixel_snap*/)
{
    float yoffs = 0;

    if(alignment & align::top)
    {
        yoffs = -miny;
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

    if(alignment & align::middle)
    {
        yoffs = (-miny -maxy) / 2.0f;
    }

    return yoffs;
}

std::pair<float, float> text::get_alignment_offsets(
        align_t alignment,
        float minx, float miny, float miny_baseline,
        float maxx, float maxy, float maxy_baseline,
        bool pixel_snap)
{
    return {get_alignment_x(alignment, minx, maxx, pixel_snap),
            get_alignment_y(alignment, miny, miny_baseline, maxy, maxy_baseline, pixel_snap)};
}

text::text() = default;
text::text(const text&) = default;
text& text::operator=(const text&) = default;
text::text(text&&) noexcept = default;
text& text::operator=(text&&) noexcept = default;
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
    regen_unicode_text();
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
    if(color_top_ == top && color_bot_ == bot)
    {
        return;
    }
    color_top_ = top;
    color_bot_ = bot;

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
    if(math::all(math::epsilonEqual(advance, advance_, math::epsilon<float>())))
    {
        return;
    }

    advance_ = advance;
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

const std::vector<line_metrics>& text::get_lines_metrics() const
{
    get_geometry();
    return lines_metrics_;
}

bool text::is_valid() const
{
    return !utf8_text_.empty() && font_;
}

float text::get_advance_offset_y() const
{
    if(font_ && font_->sdf_spread > 0)
    {
        return advance_.y + (outline_width_ * float(font_->sdf_spread) * 2.0f);
    }

    return advance_.y;
}

void text::set_kerning(bool enabled)
{
    if(kerning_enabled_ == enabled)
    {
        return;
    }
    kerning_enabled_ = enabled;
    clear_geometry();
}

void text::set_leaning(float leaning)
{
    leaning = math::clamp(leaning, -45.0f, 45.0f);
    if(math::epsilonEqual(leaning_, leaning, math::epsilon<float>()))
    {
        return;
    }
    leaning_ = leaning;
    clear_geometry();
}

float text::get_advance_offset_x() const
{
    if(font_ && font_->sdf_spread > 0)
    {
        return advance_.x + (outline_width_ * float(font_->sdf_spread) * 2.0f);
    }

    return advance_.x;
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
}

void text::clear_lines()
{
    chars_ = 0;
    lines_.clear();
}

void text::update_lines() const
{
    // if we already generated lines
    if(!lines_.empty())
    {
        return;
    }

    if(unicode_text_.empty())
    {
        return;
    }

    if(!font_)
    {
        return;
    }

    // find newlines
    float advance = 0.0f;
    auto last_space = size_t(-1);

    lines_.clear();
    cache<text>::get(lines_, 1);
    lines_.resize(1);

    const auto unicode_text_size = unicode_text_.size();
    cache<text>::get(lines_.back(), unicode_text_size);
    lines_.back().reserve(unicode_text_size);

    auto max_width = float(max_width_);
    auto advance_offset_x = get_advance_offset_x();

    for(size_t i = 0; i < unicode_text_size; ++i)
    {
        uint32_t c = unicode_text_[i];

        if(c == 32 || c == '\n')
        {
            last_space = i;
        }

        const auto& g = font_->get_glyph(c);

        bool exceedsmax_width = max_width > 0 && (advance + g.advance_x + advance_offset_x) > max_width;

        if(c == '\n' || (exceedsmax_width && (last_space != size_t(-1))))
        {
            lines_.back().resize(lines_.back().size() - (i - last_space));
            chars_ -= uint32_t(i - last_space);

            i = last_space;
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
            advance += g.advance_x;
        }
    }
}


void text::regen_unicode_text()
{
    unicode_text_.clear();
    cache<text>::get(unicode_text_, utf8_text_.size() * 2);
    unicode_text_.reserve(utf8_text_.size() * 2); // enough for most scripts

    const char* beg = utf8_text_.c_str();
    const char* const end = beg + utf8_text_.size();

    while(beg < end)
    {
        uint32_t uchar;

        int m = fnt::text_char_from_utf8(&uchar, beg, end);
        if(!m)
            break;
        unicode_text_.push_back(uchar);
        beg += m;
    }
}

void text::update_geometry(bool all) const
{
    if(!font_)
    {
        return;
    }

    if(unicode_text_.empty())
    {
        return;
    }

    if(!geometry_.empty())
    {
        return;
    }

    auto advance_offset_x = get_advance_offset_x();
    auto advance_offset_y = get_advance_offset_y();
    const auto& lines = get_lines();

    float leaning = 0.0f;
    bool has_leaning = false;
    const auto pixel_snap = font_->pixel_snap;
    const auto line_height = font_->line_height + advance_offset_y;
    const auto ascent = font_->ascent;
    const auto descent = font_->descent;
    const auto height = ascent - descent;
    const auto line_gap = line_height - height;
    const auto x_height = font_->x_height;

    if(all)
    {
        cache<text>::get(lines_metrics_, lines.size());
        lines_metrics_.reserve(lines.size());
        cache<text>::get(geometry_, chars_ * vertices_per_quad);
        geometry_.resize(chars_ * vertices_per_quad);

        has_leaning = math::epsilonNotEqual(leaning_, 0.0f, math::epsilon<float>());
        if(has_leaning)
        {
            leaning = math::rotateZ(math::vec3{0.0f, ascent, 0.0f}, math::radians(-leaning_)).x;
        }
    }
    size_t offset = 0;
    auto vptr = geometry_.data();

    const math::vec4 vcolor_top{color_top_.r, color_top_.g, color_top_.b, color_top_.a};
    const math::vec4 vcolor_bot{color_bot_.r, color_bot_.g, color_bot_.b, color_bot_.a};
    const bool has_gradient = color_top_ != color_bot_;

    float max_h = (lines.size() * line_height) - line_gap;
    float min_y = 0.0f;
    float max_y = max_h;
    float min_y_baseline = min_y + ascent;
    float max_y_baseline = max_y + descent;

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();

    float align_y = get_alignment_y(alignment_,
                                    min_y, min_y_baseline,
                                    max_y, max_y_baseline,
                                    pixel_snap);

    min_y += align_y;
    max_y += align_y;

    // Set glyph positions on a (0,0) baseline.
    // (x0,y0) for a glyph is the bottom-lefts
    auto pen_x = 0.0f;
    auto pen_y = ascent + align_y;

    for(const auto& line : lines)
    {
        line_metrics line_info{};
        line_info.minx = pen_x;
        line_info.ascent = pen_y - ascent;
        line_info.median = pen_y - x_height;
        line_info.baseline = pen_y;
        line_info.descent = pen_y - descent;

        size_t vtx_count{};
        auto last_codepoint = char_t(-1);
        for(auto c : line)
        {
            const auto& g = font_->get_glyph(c);

            if(kerning_enabled_)
            {
                // modify the pen advance with the kerning from the previous character
                pen_x += font_->get_kerning(last_codepoint, g.codepoint);
                last_codepoint = g.codepoint;
            }

            if(all)
            {
                float leaning0 = 0.0f;
                float leaning1 = 0.0f;

                if(has_leaning)
                {
                    const auto y0_offs = g.y0 + ascent;
                    const auto y0_factor = 1.0f - y0_offs / ascent;
                    leaning0 = leaning * y0_factor;

                    const auto y1_offs = g.y1 + ascent;
                    const auto y1_factor = 1.0f - y1_offs / ascent;
                    leaning1 = leaning * y1_factor;
                }

                const auto x0 = pen_x + g.x0;
                const auto x1 = pen_x + g.x1;
                const auto y0 = pen_y + g.y0;
                const auto y1 = pen_y + g.y1;

                const auto coltop = has_gradient ? get_gradient(vcolor_top, vcolor_bot, y0 - line_info.ascent, height) : color_top_;
                const auto colbot = has_gradient ? get_gradient(vcolor_top, vcolor_bot, y1 - line_info.ascent, height) : color_bot_;

                std::array<vertex_2d, 4> quad =
                {{
                    {{x0 + leaning0, y0}, {g.u0, g.v0}, coltop},
                    {{x1 + leaning0, y0}, {g.u1, g.v0}, coltop},
                    {{x1 + leaning1, y1}, {g.u1, g.v1}, colbot},
                    {{x0 + leaning1, y1}, {g.u0, g.v1}, colbot}
                }};

                std::memcpy(vptr, quad.data(), quad.size() * sizeof(vertex_2d));
                vptr += vertices_per_quad;
                vtx_count += vertices_per_quad;
            }

            pen_x += g.advance_x + advance_offset_x;

        }

        line_info.maxx = pen_x;

        if(all)
        {
            auto align_x = get_alignment_x(alignment_,
                                           line_info.minx,
                                           line_info.maxx,
                                           pixel_snap);

            if(math::epsilonNotEqual(align_x, 0.0f, math::epsilon<float>()) || pixel_snap)
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
            }

            line_info.minx += align_x;
            line_info.maxx += align_x;

            lines_metrics_.emplace_back(line_info);

            offset += vtx_count;
        }

        min_x = std::min(line_info.minx, min_x);
        max_x = std::max(line_info.maxx, max_x);

        // go to next line
        pen_x = 0;
        pen_y += line_height;
    }

    if(all)
    {
        rect_.x = min_x;
        rect_.y = min_y;
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

}
