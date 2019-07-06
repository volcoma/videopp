#include "text.h"
#include "font.h"
#include <fontpp/font.h>
#include <algorithm>
#include <iostream>

namespace video_ctrl
{


namespace
{

const size_t VERTICES_PER_QUAD = 4;

float get_alignment_miny(text::alignment alignment,
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

float get_alignment_maxy(text::alignment alignment,
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

std::pair<float, float> get_alignment_offsets(text::alignment alignment,
        float minx, float miny, float maxx, float maxy)
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
            xoffs = (-minx - maxx) / 2;
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
            yoffs = (-miny - maxy) / 2;
            break;
    }
    return {xoffs, yoffs};
}

}

text::text() = default;
text::~text() = default;

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

void text::set_color(color c)
{
    if(color_ == c)
    {
        return;
    }
    color_ = c;
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
    if(shadow_color_ == c)
    {
        return;
    }
    shadow_color_ = c;
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

void text::set_max_width(float w)
{
    if(math::epsilonEqual(max_width_, w, math::epsilon<float>()))
    {
        return;
    }

    max_width_ = w;
    clear_lines();
    clear_geometry();
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

const std::vector<float>& text::get_ascent_lines() const
{
    get_geometry();
    return ascent_lines_;
}

const std::vector<float>& text::get_descent_lines() const
{
    get_geometry();
    return descent_lines_;
}

const std::vector<float>& text::get_baseline_lines() const
{
    get_geometry();
    return baseline_lines_;
}

bool text::is_valid() const
{
    return !utf8_text_.empty() && font_;
}

float text::get_advance_offset_y() const
{
    if(font_ && font_->sdf_spread > 0)
    {
        return advance_.y + (outline_width_ * 10.0f);
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

float text::get_advance_offset_x() const
{
    if(font_ && font_->sdf_spread > 0)
    {
        return advance_.x + (outline_width_ * 10.0f);
    }

    return advance_.x;
}

const std::string& text::get_utf8_text() const
{
    return utf8_text_;
}

rect align_rect(const rect& r, text::alignment align)
{
    auto minx = static_cast<float>(r.x);
    auto maxx = static_cast<float>(r.x + r.w);
    auto miny = static_cast<float>(r.y);
    auto maxy = static_cast<float>(r.y + r.h);

    auto offsets = get_alignment_offsets(align, minx, miny, maxx, maxy);
    float xoffs = offsets.first;
    float yoffs = offsets.second;

    auto result = r;
    result.x += static_cast<int>(xoffs);
    result.y += static_cast<int>(yoffs);
    return result;
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
    ascent_lines_.clear();
    descent_lines_.clear();
    baseline_lines_.clear();
    rect_ = {};
    min_baseline_height_ = {};
    max_baseline_height_ = {};
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

    // find newlines
    auto last_space = size_t(-1);

    lines_.clear();
    lines_.resize(1);
    lines_.back().reserve(unicode_text_.size());

    for(size_t i = 0; i < unicode_text_.size(); ++i)
    {
        uint32_t c = unicode_text_[i];

        if(c == 32 || c == '\n')
        {
            last_space = i;
        }

        if(c == '\n')
        {
            lines_.back().resize(lines_.back().size() - (i - last_space));
            chars_ -= uint32_t(i - last_space);

            i = last_space;
            lines_.resize(lines_.size() + 1);
            lines_.back().reserve(unicode_text_.size() - chars_);
            last_space = size_t(-1);
        }
        else
        {
            lines_.back().push_back(c);
            ++chars_;

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


void text::update_geometry() const
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
    geometry_.resize(chars_ * VERTICES_PER_QUAD);
    auto vptr = geometry_.data();
    size_t offset = 0;

    auto line_height = font_->line_height + advance_offset_y;
    auto ascent = font_->ascent;
    auto descent = font_->descent;
    auto height = ascent - descent;
    auto baseline = ascent;
    float xadvance = 0;
    float yadvance = baseline;
    for(const auto& line : lines)
    {
        size_t line_offset = 0;
        float line_minx = 100000.0f;
        float line_maxx = -100000.0f;

        auto last_codepoint = char_t(-1);
        for(auto c : line)
        {
            const auto& g = font_->get_glyph(c);

            if(kerning_enabled_)
            {
                // modify the xadvance with the kerning from the previous character
                xadvance += font_->get_kerning(last_codepoint, g.codepoint);
                last_codepoint = g.codepoint;
            }

            auto x0 = g.x0 + xadvance;
            auto x1 = g.x1 + xadvance;
            auto y0 = g.y0 + yadvance;
            auto y1 = g.y1 + yadvance;
            *vptr++ = {{x0 + lean_, y0}, {g.u0, g.v0}, color_};
            *vptr++ = {{x1 + lean_, y0}, {g.u1, g.v0}, color_};
            *vptr++ = {{x1 - lean_, y1}, {g.u1, g.v1}, color_};
            *vptr++ = {{x0 - lean_, y1}, {g.u0, g.v1}, color_};

            line_offset += 4;

            xadvance += g.advance_x + advance_offset_x;
            line_minx = std::min(x0, line_minx);
            line_maxx = std::max(x1, line_maxx);
            line_maxx = std::max(line_maxx, xadvance);
        }

        auto line_ascent_miny = -ascent + yadvance;
        auto line_baseline_miny = yadvance;
        auto line_miny = get_alignment_miny(alignment_, line_ascent_miny, line_baseline_miny);

        auto line_descent_maxy = line_ascent_miny + height;
        auto line_baseline_maxy = line_ascent_miny + ascent;
        auto line_maxy = get_alignment_maxy(alignment_, line_descent_maxy, line_baseline_maxy);

        auto offsets = get_alignment_offsets(alignment_, line_minx, line_miny, line_maxx, line_maxy);
        float xoffs = offsets.first;

        auto v = geometry_.data() + offset;
        for(size_t i = 0; i < line_offset; ++i)
        {
            auto& vv = *(v+i);
            vv.pos.x += xoffs;
        }

        minx = std::min(line_minx, minx);
        maxx = std::max(line_maxx, maxx);

        maxy_descent = std::max(line_descent_maxy, maxy_descent);
        maxy_baseline = std::max(line_baseline_maxy, maxy_baseline);

        miny_ascent = std::min(line_ascent_miny, miny_ascent);
        miny_baseline = std::min(line_baseline_miny, miny_baseline);

        ascent_lines_.emplace_back(line_ascent_miny);
        descent_lines_.emplace_back(line_descent_maxy);
        baseline_lines_.emplace_back(line_baseline_maxy);

        offset += line_offset;
        yadvance += line_height;
        xadvance = advance_offset_x;
    }

    auto miny = get_alignment_miny(alignment_, miny_ascent, miny_baseline);
    auto maxy = get_alignment_maxy(alignment_, maxy_descent, maxy_baseline);

    auto offsets = get_alignment_offsets(alignment_, minx, miny, maxx, maxy);
    float yoffs = offsets.second;

    for(auto& v : geometry_)
    {
        auto& pos = v.pos;
        pos.y += yoffs;

        rect_.insert({pos.x, pos.y});
    }

    for(auto& line : ascent_lines_)
    {
        line += yoffs;
    }
    for(auto& line : descent_lines_)
    {
        line += yoffs;
    }
    for(auto& line : baseline_lines_)
    {
        line += yoffs;
    }
    min_baseline_height_ = miny_baseline - miny_ascent + shadow_offsets_.y;
    max_baseline_height_ = maxy_baseline - miny_ascent + shadow_offsets_.y;
    rect_.h = maxy_descent - miny_ascent + shadow_offsets_.y;
    rect_.w = maxx - minx + shadow_offsets_.x;
}

float text::get_width() const
{
    if(!font_)
    {
        return 0.0f;
    }
    if(rect_)
    {
        return rect_.w;
    }

    update_geometry();

    return rect_.w;
}

float text::get_height() const
{
    if(!font_)
    {
        return 0.0f;
    }
    if(rect_)
    {
        return rect_.h;
    }

    update_geometry();

    return rect_.h;
}

float text::get_min_baseline_height() const
{
    if(!font_)
    {
        return 0.0f;
    }
    if(min_baseline_height_ != 0.0f)
    {
        return min_baseline_height_;
    }

    update_geometry();

    return min_baseline_height_;
}

float text::get_max_baseline_height() const
{
    if(!font_)
    {
        return 0.0f;
    }
    if(max_baseline_height_ != 0.0f)
    {
        return max_baseline_height_;
    }

    update_geometry();

    return max_baseline_height_;
}

rect text::get_rect() const
{
    if(!font_)
    {
        return {};
    }
    if(rect_)
    {
        return cast_rect(rect_);
    }
    update_geometry();

    return cast_rect(rect_);
}

const color& text::get_color() const
{
    return color_;
}

const color& text::get_outline_color() const
{
    if(outline_width_ > 0.0f)
    {
        return outline_color_;
    }

    return get_color();
}

float text::get_outline_width() const
{
    return outline_width_;
}

const math::vec2& text::get_shadow_offsets() const
{
    return shadow_offsets_;
}

const color& text::get_shadow_color() const
{
    return shadow_color_;
}
}
