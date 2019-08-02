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
    return {xoffs, yoffs};
}
color get_gradient(const color& ct, const color& cb, float t, float dt)
{
    t = std::max(t, 0.0f);
    t = std::min(t, dt);
    const float fa = t/dt;
    const float fc = (1.f-fa);
    return {
        uint8_t(float(ct.r) * fc + float(cb.r) * fa),
        uint8_t(float(ct.g) * fc + float(cb.g) * fa),
        uint8_t(float(ct.b) * fc + float(cb.b) * fa),
        uint8_t(float(ct.a) * fc + float(cb.a) * fa)
    };
}

color shade_verts_linear_color_gradient(math::vec2 pos,
                                        math::vec2 gradient_p0,
                                        math::vec2 gradient_p1,
                                        color col0,
                                        color col1)
{
    auto gradient_extent = gradient_p1 - gradient_p0;
    float gradient_inv_length2 = 1.0f / math::length2(gradient_extent);

    float d = math::dot(pos - gradient_p0, gradient_extent);
    float t = math::clamp(d * gradient_inv_length2, 0.0f, 1.0f);

    auto r = math::lerp(float(col0.r), float(col1.r), t);
    auto g = math::lerp(float(col0.g), float(col1.g), t);
    auto b = math::lerp(float(col0.b), float(col1.b), t);
    auto a = math::lerp(float(col0.a), float(col1.a), t);

    color col{};
    col.r = uint8_t(r);
    col.g = uint8_t(g);
    col.b = uint8_t(b);
    col.a = uint8_t(a);
    return col;

}

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

void text::set_color(color c)
{
    set_vgradient_color(c, c);
}

void text::set_vgradient_color(color begin, color end)
{
    if(color_begin_ == begin && color_end_ == end)
    {
        return;
    }
    color_begin_ = begin;
    color_end_ = end;

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

const std::vector<text::line_metrics>& text::get_lines_metrics() const
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

void text::set_leaning(float leaning)
{
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
        return advance_.x + (outline_width_ * 10.0f);
    }

    return advance_.x;
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
    auto x_height = font_->x_height;
    auto ascent = font_->ascent;
    auto descent = font_->descent;
    auto height = ascent - descent;
    auto baseline = ascent;

    // Set glyph positions on a (0,0) baseline.
    // (x0,y0) for a glyph is the bottom-lefts
    auto pen_x = 0.0f;
    auto pen_y = baseline;

    for(const auto& line : lines)
    {
        size_t vtx_count{};

        line_metrics line_info{};
        line_info.minx = pen_x;

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

            auto h0 = std::abs(g.y0);
            auto h0_factor = h0 / std::max(x_height, 1.0f);
            auto leaning0 = leaning_ * h0_factor;

            auto h1 = std::abs(g.y1);
            auto h1_factor = h1 / std::max(x_height, 1.0f);
            auto leaning1 = leaning_ * h1_factor;

            auto x0 = pen_x + g.x0;
            auto x1 = pen_x + g.x1;
            auto y0 = pen_y + g.y0;
            auto y1 = pen_y + g.y1;

            *vptr++ = {{x0 + leaning0, y0}, {g.u0, g.v0}, color_begin_};
            *vptr++ = {{x1 + leaning0, y0}, {g.u1, g.v0}, color_begin_};
            *vptr++ = {{x1 - leaning1, y1}, {g.u1, g.v1}, color_end_};
            *vptr++ = {{x0 - leaning1, y1}, {g.u0, g.v1}, color_end_};

            vtx_count += 4;

            pen_x += g.advance_x + advance_offset_x;

        }

        line_info.maxx = pen_x;

        line_info.ascent = pen_y - ascent;
        line_info.baseline = pen_y;
        line_info.descent = line_info.ascent + height;

        line_info.miny = get_alignment_miny(alignment_, line_info.ascent, line_info.baseline);
        line_info.maxy = get_alignment_maxy(alignment_, line_info.descent, line_info.baseline);

        minx = std::min(line_info.minx, minx);
        maxx = std::max(line_info.maxx, maxx);

        maxy_descent = std::max(line_info.descent, maxy_descent);
        maxy_baseline = std::max(line_info.baseline, maxy_baseline);

        miny_ascent = std::min(line_info.ascent, miny_ascent);
        miny_baseline = std::min(line_info.baseline, miny_baseline);

        auto align_offsets = get_alignment_offsets(alignment_, line_info.minx, line_info.miny, line_info.maxx, line_info.maxy);
        auto align_x = align_offsets.first;

        auto v = geometry_.data() + offset;
        for(size_t i = 0; i < vtx_count; ++i)
        {
            auto& vv = *(v+i);
            vv.pos.x += align_x;

            math::vec2 p0{minx, line_info.ascent};
            math::vec2 p1{minx, line_info.descent - line_info.ascent};
//            math::vec2 p2{maxx, line_info.descent - line_info.ascent};
//            math::vec2 dir = p2 - p0;//{1.0f, 1.0f};
//            auto a = p0 + math::normalize(dir);
//            auto b = p1 + math::vec2(1.0f, 0.0f);

//            auto c = b - a;
//            auto t = math::dot(c, b) / math::dot(a, b);
//            auto intersection = a * t;
            vv.col = shade_verts_linear_color_gradient(vv.pos, p0, p1, color_begin_, color_end_);
        }

        offset += vtx_count;

        line_info.minx += align_x;
        line_info.maxx += align_x;

        lines_metrics_.emplace_back(line_info);

        // go to next line
        pen_x = 0;
        pen_y += line_height;
    }

    auto miny = get_alignment_miny(alignment_, miny_ascent, miny_baseline);
    auto maxy = get_alignment_maxy(alignment_, maxy_descent, maxy_baseline);

    auto align_offsets = get_alignment_offsets(alignment_, minx, miny, maxx, maxy);
    auto align_x = align_offsets.first;
    auto align_y = align_offsets.second;

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

    rect_.x = align_x;
    rect_.y = align_y;
    rect_.h = maxy_descent - miny_ascent + shadow_offsets_.y;
    rect_.w = maxx - minx + shadow_offsets_.x;
}

float text::get_width() const
{
    if(rect_)
    {
        return rect_.w;
    }

    update_geometry();

    return rect_.w;
}

float text::get_height() const
{
    if(rect_)
    {
        return rect_.h;
    }

    update_geometry();

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
    if(rect_)
    {
        return rect_;
    }

    update_geometry();

    return rect_;
}

const color& text::get_outline_color() const
{
    if(outline_width_ > 0.0f)
    {
        return outline_color_;
    }

    static color fallback{0, 0, 0, 0};
    return fallback;
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
