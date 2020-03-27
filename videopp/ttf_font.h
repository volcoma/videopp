#pragma once

#include "glyph_range.h"

#include "font_info.h"

namespace gfx
{

font_info create_font_from_ttf(
    const std::string& path, // font path
    const glyphs& codepoint_ranges, // ranges to rasterize
    float font_size, // (point) size to rasterize
    bool kerning = false
);

font_info create_default_font(
	float font_size = 13 // (point) size to rasterize
);

}
