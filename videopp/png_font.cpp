#include <memory>

#include "png_font.h"

#include <cstdlib>
#include <sstream>

using namespace std;

namespace gfx
{

namespace
{

void report_error(const std::string& name, char_t c)
{
    std::stringstream ss;
    ss << "Could not find codepoint : '";

    if(c > 0 && c < 128)
        ss << char(c);
    else
        ss << c;

    ss << "' in PNG font [" << name << "]. Probably caused by wrong specified range, missing cyan pixel or wrong rect.";

    throw gfx::exception(ss.str());
}
bool is_cyan(color c)
{
    return c.r == 0 && c.g == 255 && c.b == 255 && c.a != 0;
}
int next_cyan_on_line(surface* s, const point& p, int limit)
{
    if (limit == -1)
    {
        limit = s->get_width();
    }
    for(int i = p.x + 1; i < limit; ++i)
    {
        const point sample_point = {i, p.y};
        auto sample = s->get_pixel(sample_point);
        if (is_cyan(sample))
        {
            s->set_pixel(sample_point, {0,0,0,0});
            return i;
        }
    }
    return -1;
}


}

font_info create_font_from_cyan_sep_png(const std::string& name, const std::string& filename, int font_size,
                                        const glyphs& codepoint_ranges, const rect& symbols_rect)
{
    auto surf = std::make_unique<surface>(filename);
    return create_font_from_cyan_sep_png(name, std::move(surf), font_size, codepoint_ranges, symbols_rect);
}


font_info create_font_from_cyan_sep_png(const std::string& name, std::unique_ptr<surface>&& surface, int font_size,
                                        const glyphs& codepoint_ranges, const rect& symbols_rect)
{
    font_info f;
    f.face_name = name;
    f.sdf_spread = 0;
    f.pixel_snap = true;
    f.surface = std::move(surface);

    auto s = f.surface.get();

    auto rect = symbols_rect;

    // if not specified then use the whole texture
    if(!rect)
    {
        rect = s->get_rect();
    }


    point start_point {rect.x, rect.y};
    s->set_pixel(start_point, {0, 0, 0, 0});

    int height = font_size;
    f.line_height = f.size = float(height);
    f.ascent = f.line_height;
    f.descent = 0;

    char_t max_char = 0;
    size_t total_glyphs = 0;
    for (auto& range : codepoint_ranges)
    {
        total_glyphs += range.second - range.first + 1;
        max_char = std::max(max_char, char_t(range.second + 1));
    }

    f.glyph_index.resize(max_char, char_t(-1));
    f.glyphs.reserve(total_glyphs);
    for (auto& range : codepoint_ranges)
    {
        for (auto c = range.first; c < range.second + 1; ++c)
        {
            int x = 0;
            while((x = next_cyan_on_line(s, start_point, -1)) == -1)
            {
                start_point = {rect.x, start_point.y + height + 1};
                if((start_point.y >= rect.y + rect.h) || (start_point.y >= s->get_rect().h))
                {
                    report_error(name, c);
                }
            }

            f.glyph_index[c] = char_t(f.glyphs.size());
            f.glyphs.emplace_back();
            auto& g = f.glyphs.back();

            g.codepoint = c;

            // offset 1 for the cyan pixel
            const int offset = 1;

            auto symbol_start_x = float(start_point.x);
            //Turns out we only need to offset Y, the cyan pixel can overlap actual data at the X axis
            auto symbol_start_y = float(start_point.y + offset);
            auto width = float(x) - symbol_start_x;
            g.x0 = 0.0f;
            g.y0 = -f.ascent;
            g.x1 = float(width);
            g.y1 = g.y0 + float(height);

            g.advance_x = g.x1;

            g.u0 = symbol_start_x / float(s->get_width());
            g.v0 = symbol_start_y / float(s->get_height());

            g.u1 = float(x) / float(s->get_width());
            g.v1 = (symbol_start_y + float(height)) / float(s->get_height());

            start_point.x = x;
        }
    }


    f.cap_height = f.ascent;
    f.x_height = f.cap_height * 0.5f;

    return f;
}

}
