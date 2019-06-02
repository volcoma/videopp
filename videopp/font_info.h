#pragma once

#include "math/glm_includes.h"
#include "surface.h"
#include "logger.h"

#include <cstdint>
#include <vector>
#include <locale>
#include <codecvt>

namespace video_ctrl
{

using char_t = uint32_t;

struct font_info
{
    struct glyph
    {
        uint32_t codepoint = 0; // Unicode codepoint (!= utf-8 symbol)
        float xadvance = 0.0f; // spacing to next character
        math::vec2 xy0 = {0.0f, 0.0f};
        math::vec2 xy1 = {0.0f, 0.0f}; // geometry (relative to 0)
        math::vec2 uv0 = {0.0f, 0.0f};
        math::vec2 uv1 = {0.0f, 0.0f};// texture coordinates


    };

    const glyph& get_glyph(uint32_t codepoint) const
    {
        if (codepoint >= uint32_t(glyph_index.size()) || glyph_index[codepoint] == -1)
        {
            return fallback_glyph;
        }

        return glyphs[size_t(glyph_index[codepoint])];
    }
    std::string face_name; // name of font
    float size = 0; // (point) size based on which the glyphs will be rasterized onto the texture

    int sdf_spread = 0; // spread of signed distance field (0 if no distance field is applied)

    float pixel_density = 0; // number of texture pixels per unit of geometry
                         // both horizontal and vertical
                         // this, of course, relies on several assumptions
                         // (which hold true for the current loaders)
                         // but may not necessarily do so for others
                         // 1. horizontal and vertical density ARE the same
                         // 2. all characters have the same pixel density

    float line_height = 0; // height of a single line
    float ascent = 0; // ascent of letters
    float descent = 0; // descent of letters

    std::vector<glyph> glyphs;
    std::vector<int> glyph_index; // sparse vector of indices per codepoint

    float max_xadvance = 0; // useful when drawing fixed-width numeric characters

    glyph fallback_glyph; // glyph to be used when requesting a non-existent one

    surface_ptr surface; // rasterized font goes here
};

using font_info_ptr = std::shared_ptr<font_info>;

}

