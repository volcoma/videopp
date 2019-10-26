#pragma once

#include "glyph_range.h"

#include "font_info.h"

namespace gfx
{

font_info create_font_from_cyan_sep_png(
    const std::string& name, // face name
    std::unique_ptr<surface>&& surface, // png surface
    int font_size, // size of the font
    const glyphs& codepoint_ranges, // ranges in png
    const rect& symbols_rect = {} // part of the texture which is used
);

font_info create_font_from_cyan_sep_png(
    const std::string& name, // face name
    const std::string& filename, // png file name
    int font_size, // size of the font
    const glyphs& codepoint_ranges, // ranges in png
    const rect& symbols_rect = {} // part of the texture which is used
);

}
