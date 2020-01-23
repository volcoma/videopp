#pragma once

#include "math/glm_includes.h"
#include "surface.h"
#include "logger.h"

#include <fontpp/font.h>
#include <cstdint>
#include <vector>
#include <locale>
#include <codecvt>

namespace gfx
{

using char_t = fnt::font_wchar;
using kerning_table_t = fnt::kerning_table;
using glyph = fnt::font_glyph;

struct font_info
{
    const glyph& get_glyph(uint32_t codepoint) const
    {
        if (codepoint >= uint32_t(glyph_index.size()) || glyph_index[codepoint] == char_t(-1))
        {
            return fallback_glyph;
        }

        return glyphs[size_t(glyph_index[codepoint])];
    }

    float get_kerning(uint32_t codepoint1, uint32_t codepoint2) const
    {
        auto it = kernings.find({char_t(codepoint1), char_t(codepoint2)});

        if(it != std::end(kernings))
        {
            return it->second;
        }

        return 0.0f;
    }

    /// name of font
    std::string face_name;

    /// all loaded glyphs
    std::vector<glyph> glyphs;

    /// sparse vector of indices per codepoint
    std::vector<char_t> glyph_index;

    /// kerning lookup table per codepoint
    kerning_table_t kernings{};

    /// glyph to be used when requesting a non-existent one
    glyph fallback_glyph{};

    /// rasterized font goes here
    surface_ptr surface;

    /// (point) size based on which the glyphs will be rasterized onto the texture
    /// Do not use this for line calculations - use line_height
    float size = 0;
    /// height of a single line
    float line_height = 0;
    /// ascent of letters
    float ascent = 0;
    /// descent of letters
    float descent = 0;
    /// height of x glyph.
    float x_height = 0;
    float cap_height = 0;
    float superscript_size = 0;
    float superscript_offset = 0;
    float subscript_size = 0;
    float subscript_offset = 0;
    /// spread of signed distance field (0 if no distance field is applied)
    uint32_t sdf_spread = 0;

    bool pixel_snap{};
};

using font_info_ptr = std::shared_ptr<font_info>;

}

