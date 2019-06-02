#include "text.h"
#include "font.h"
#include <algorithm>
#include <iostream>

namespace video_ctrl
{


namespace
{

const size_t VERTICES_PER_QUAD = 4;

float align_to_pixel(float val)
{
    return val;//static_cast<float>(static_cast<int>(val));
}

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
    return {align_to_pixel(xoffs), align_to_pixel(yoffs)};
}

// Convert UTF-8 to 32-bits character, process single character input.
// Based on stb_from_utf8() from github.com/nothings/stb/
// We handle UTF-8 decoding error by skipping forward.
// Returns how many chars have been read from the in_text
int unicode_char_from_utf8(uint32_t* out_char, const char* in_text, const char* in_text_end)
{
    uint32_t c = uint32_t(-1);
    const unsigned char* str = (const unsigned char*)in_text;
    if(!(*str & 0x80))
    {
        c = (unsigned int)(*str++);
        *out_char = c;
        return 1;
    }
    if((*str & 0xe0) == 0xc0)
    {
        *out_char = 0xFFFD; // will be invalid but not end of string
        if(in_text_end && in_text_end - (const char*)str < 2)
            return 0;
        if(*str < 0xc2)
            return 2;
        c = (unsigned int)((*str++ & 0x1f) << 6);
        if((*str & 0xc0) != 0x80)
            return 2;
        c += (*str++ & 0x3f);
        *out_char = c;
        return 2;
    }
    if((*str & 0xf0) == 0xe0)
    {
        *out_char = 0xFFFD; // will be invalid but not end of string
        if(in_text_end && in_text_end - (const char*)str < 3)
            return 0;
        if(*str == 0xe0 && (str[1] < 0xa0 || str[1] > 0xbf))
            return 3;
        if(*str == 0xed && str[1] > 0x9f)
            return 3; // str[1] < 0x80 is checked below
        c = (unsigned int)((*str++ & 0x0f) << 12);
        if((*str & 0xc0) != 0x80)
            return 3;
        c += (unsigned int)((*str++ & 0x3f) << 6);
        if((*str & 0xc0) != 0x80)
            return 3;
        c += (*str++ & 0x3f);
        *out_char = c;
        return 3;
    }
    if((*str & 0xf8) == 0xf0)
    {
        *out_char = 0xFFFD; // will be invalid but not end of string
        if(in_text_end && in_text_end - (const char*)str < 4)
            return 0;
        if(*str > 0xf4)
            return 4;
        if(*str == 0xf0 && (str[1] < 0x90 || str[1] > 0xbf))
            return 4;
        if(*str == 0xf4 && str[1] > 0x8f)
            return 4; // str[1] < 0x80 is checked below
        c = (unsigned int)((*str++ & 0x07) << 18);
        if((*str & 0xc0) != 0x80)
            return 4;
        c += (unsigned int)((*str++ & 0x3f) << 12);
        if((*str & 0xc0) != 0x80)
            return 4;
        c += (unsigned int)((*str++ & 0x3f) << 6);
        if((*str & 0xc0) != 0x80)
            return 4;
        c += (*str++ & 0x3f);
        // utf-8 encodings of values used in surrogate pairs are invalid
        if((c & 0xFFFFF800) == 0xD800)
            return 4;
        *out_char = c;
        return 4;
    }
    *out_char = 0;
    return 0;
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

const std::vector<float>&text::get_ascent_lines() const
{
    get_geometry();
    return ascent_lines_;
}

const std::vector<float>&text::get_descent_lines() const
{
    get_geometry();
    return descent_lines_;
}

const std::vector<float>&text::get_baseline_lines() const
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
}

void text::clear_lines()
{
    rect_ = {};

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
    float advance = 0;
    auto last_space = size_t(-1);

    lines_.clear();
    lines_.resize(1);
    lines_.back().reserve(unicode_text_.size());

    auto max_width = float(max_width_);
    auto advance_offset_x = get_advance_offset_x();

    for(size_t i = 0; i < unicode_text_.size(); ++i)
    {
        uint32_t c = unicode_text_[i];

        if(c == 32 || c == '\n')
        {
            last_space = i;
        }

        const auto& g = font_->get_glyph(c);

        bool exceedsmax_width = max_width > 0 && (advance + g.xadvance + advance_offset_x) > max_width;

        if(c == '\n' || (exceedsmax_width && (last_space != size_t(-1))))
        {
            lines_.back().resize(lines_.back().size() - (i - last_space));
            chars_ -= uint32_t(i - last_space);

            i = last_space;
            lines_.resize(lines_.size() + 1);
            lines_.back().reserve(unicode_text_.size() - chars_);
            advance = 0;
            last_space = size_t(-1);
        }
        else
        {
            lines_.back().push_back(c);
            ++chars_;
            advance += g.xadvance;
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

        int m = unicode_char_from_utf8(&uchar, beg, end);
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

        for(auto c : line)
        {
            const auto& g = font_->get_glyph(c);

            auto x0 = align_to_pixel(g.xy0.x + xadvance);
            auto x1 = align_to_pixel(g.xy1.x + xadvance);
            auto y0 = align_to_pixel(g.xy0.y + yadvance);
            auto y1 = align_to_pixel(g.xy1.y + yadvance);

            *vptr++ = {{x0, y0}, g.uv0, color_};
            *vptr++ = {{x1, y0}, {g.uv1.x, g.uv0.y}, color_};
            *vptr++ = {{x1, y1}, g.uv1, color_};
            *vptr++ = {{x0, y1}, {g.uv0.x, g.uv1.y}, color_};

            line_offset += 4;

            xadvance += g.xadvance + advance_offset_x;
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
