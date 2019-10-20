#include "text.h"
#include "font.h"
#include <algorithm>
#include <iostream>

namespace video_ctrl
{


namespace
{

constexpr size_t vertices_per_quad = 4;

inline float get_alignment_miny(text::alignment alignment,
        float y, float y_baseline)
{
    switch(alignment)
    {
        case text::alignment::baseline_top:
        case text::alignment::baseline_top_left:
        case text::alignment::baseline_top_right:
            return y_baseline;
        default:
            return y;
    }
}

inline float get_alignment_maxy(text::alignment alignment,
        float y, float y_baseline)
{
    switch(alignment)
    {
        case text::alignment::baseline_bottom:
        case text::alignment::baseline_bottom_left:
        case text::alignment::baseline_bottom_right:
            return y_baseline;
        default:
            return y;
    }
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
}

std::pair<float, float> text::get_alignment_offsets(text::alignment alignment,
        float minx, float miny, float maxx, float maxy, bool pixel_snap)
{
    // offset according to m_alignment
    enum class align
    {
        begin,
        center,
        end,
    };

    align ax = align::center;
    align ay = align::center;

    switch(alignment)
    {
        case text::alignment::top:
        case text::alignment::baseline_top:
            ax = align::center;
            ay = align::begin;
            break;
        case text::alignment::bottom:
        case text::alignment::baseline_bottom:
            ax = align::center;
            ay = align::end;
            break;
        case text::alignment::left:
            ax = align::begin;
            ay = align::center;
            break;
        case text::alignment::right:
            ax = align::end;
            ay = align::center;
            break;
        case text::alignment::center:
            ax = align::center;
            ay = align::center;
            break;
        case text::alignment::top_left:
        case text::alignment::baseline_top_left:
            ax = align::begin;
            ay = align::begin;
            break;
        case text::alignment::top_right:
        case text::alignment::baseline_top_right:
            ax = align::end;
            ay = align::begin;
            break;
        case text::alignment::bottom_left:
        case text::alignment::baseline_bottom_left:
            ax = align::begin;
            ay = align::end;
            break;
        case text::alignment::bottom_right:
        case text::alignment::baseline_bottom_right:
            ax = align::end;
            ay = align::end;
            break;
        default:
            break;
    }

    float xoffs = 0;
    float yoffs = 0;

    switch(ax)
    {
        case align::begin:
            xoffs = -minx;
            break;
        case align::end:
            xoffs = -maxx;
            break;
        case align::center:
            xoffs = (-minx-maxx) / 2.0f;
            break;
    }

    switch(ay)
    {
        case align::begin:
            yoffs = -miny;
            break;
        case align::end:
            yoffs = -maxy;
            break;
        case align::center:
            yoffs = (-miny -maxy) / 2.0f;
            break;
    }

    if(pixel_snap)
    {
        return {float(int(xoffs)), yoffs};
    }

    return {xoffs, yoffs};
}

text::text() noexcept
{
    recover<text>(geometry_);
    recover<text>(lines_);
    recover<text>(lines_metrics_);
    recover<text>(unicode_text_);
    recover<text>(utf8_text_);
}

text::~text()
{
    recycle<text>(geometry_);
    for(auto& line : lines_)
    {
        recycle<text>(line);
    }
    recycle<text>(lines_);
    recycle<text>(lines_metrics_);
    recycle<text>(unicode_text_);
    recycle<text>(utf8_text_);
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

text::alignment text::get_alignment() const
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


void text::set_alignment(text::alignment a)
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
        return advance_.y + (outline_width_ * font_->sdf_spread * 2.0f);
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
        return advance_.x + (outline_width_ * font_->sdf_spread * 2.0f);
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
    lines_.resize(1);

    const auto unicode_text_size = unicode_text_.size();
    recover<text>(lines_.back());
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
            recover<text>(lines_.back());
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

    // used for alignment
    float minx = 100000.0f;
    float maxx = -100000.0f;
    float maxy_descent = -100000.0f;
    float maxy_baseline = -100000.0f;

    float miny_ascent = 100000.0f;
    float miny_baseline = 100000.0f;

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
    const auto baseline = ascent;

    if(all)
    {
        lines_metrics_.reserve(lines.size());
        geometry_.resize(chars_ * vertices_per_quad);

        has_leaning = math::epsilonNotEqual(leaning_, 0.0f, math::epsilon<float>());
        if(has_leaning)
        {
            leaning = math::rotateZ(math::vec3{0.0f, ascent, 0.0f}, math::radians(-leaning_)).x;
        }
    }
    size_t offset = 0;
    auto vptr = geometry_.data();

    const math::vec4 vcolor_top = color_top_;
    const math::vec4 vcolor_bot = color_bot_;
    const bool has_gradient = color_top_ != color_bot_;

    // Set glyph positions on a (0,0) baseline.
    // (x0,y0) for a glyph is the bottom-lefts
    auto pen_x = 0.0f;
    auto pen_y = baseline;

    for(const auto& line : lines)
    {
        line_metrics line_info{};
        line_info.minx = pen_x;
        line_info.ascent = pen_y - ascent;
        line_info.baseline = pen_y;
        line_info.descent = line_info.ascent + height;

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
                    const auto y0_offs = g.y0 + baseline;
                    const auto y0_factor = 1.0f - y0_offs / baseline;
                    leaning0 = leaning * y0_factor;

                    const auto y1_offs = g.y1 + baseline;
                    const auto y1_factor = 1.0f - y1_offs / baseline;
                    leaning1 = leaning * y1_factor;
                }

                const auto x0 = pen_x + g.x0;
                const auto x1 = pen_x + g.x1;
                const auto y0 = pen_y + g.y0;
                const auto y1 = pen_y + g.y1;

                const auto coltop = has_gradient ? get_gradient(vcolor_top, vcolor_bot, y0 - line_info.ascent, height) : color_top_;
                const auto colbot = has_gradient ? get_gradient(vcolor_top, vcolor_bot, y1 - line_info.ascent, height) : color_bot_;

                vertex_2d quad[vertices_per_quad] =
                {
                    {{x0 + leaning0, y0}, {g.u0, g.v0}, coltop},
                    {{x1 + leaning0, y0}, {g.u1, g.v0}, coltop},
                    {{x1 + leaning1, y1}, {g.u1, g.v1}, colbot},
                    {{x0 + leaning1, y1}, {g.u0, g.v1}, colbot}
                };

                std::memcpy(vptr, quad, sizeof(quad));
                vptr += vertices_per_quad;
                vtx_count += vertices_per_quad;
            }

            pen_x += g.advance_x + advance_offset_x;

        }

        line_info.maxx = pen_x;


        line_info.miny = get_alignment_miny(alignment_, line_info.ascent, line_info.baseline);
        line_info.maxy = get_alignment_maxy(alignment_, line_info.descent, line_info.baseline);

        if(all)
        {
            auto align_offsets = get_alignment_offsets(alignment_, line_info.minx, line_info.miny, line_info.maxx, line_info.maxy, pixel_snap);
            auto align_x = align_offsets.first;

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

            offset += vtx_count;
        }

        minx = std::min(line_info.minx, minx);
        maxx = std::max(line_info.maxx, maxx);

        maxy_descent = std::max(line_info.descent, maxy_descent);
        miny_ascent = std::min(line_info.ascent, miny_ascent);

        if(all)
        {
            maxy_baseline = std::max(line_info.baseline, maxy_baseline);
            miny_baseline = std::min(line_info.baseline, miny_baseline);
            lines_metrics_.emplace_back(line_info);
        }

        // go to next line
        pen_x = 0;
        pen_y += line_height;
    }

    if(all)
    {
        auto miny = get_alignment_miny(alignment_, miny_ascent, miny_baseline);
        auto maxy = get_alignment_maxy(alignment_, maxy_descent, maxy_baseline);

        auto align_offsets = get_alignment_offsets(alignment_, minx, miny, maxx, maxy, pixel_snap);
        auto align_y = align_offsets.second;

        if(math::epsilonNotEqual(align_y, 0.0f, math::epsilon<float>()))
        {
            for(auto& v : geometry_)
            {
                auto& pos = v.pos;
                pos.y += align_y;
            }

            for(auto& line : lines_metrics_)
            {
                line.ascent += align_y;
                line.baseline += align_y;
                line.descent += align_y;
                line.miny += align_y;
                line.maxy += align_y;
            }
        }

        rect_.x = minx;
        rect_.y = align_y;
    }

    rect_.h = maxy_descent - miny_ascent + shadow_offsets_.y;
    rect_.w = maxx - minx + shadow_offsets_.x;
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
    return first_line.baseline - first_line.ascent + shadow_offsets_.y;
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
    return last_line.baseline - first_line.ascent + shadow_offsets_.y;
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
