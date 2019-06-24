#pragma once

#include "math/glm_includes.h"
#include "surface.h"
#include "logger.h"

#include <cstdint>
#include <vector>
#include <locale>
#include <codecvt>
#include <map>

namespace video_ctrl
{

using char_t = uint16_t;
using kerning_table_t = std::vector<std::vector<float>>;

struct font_info
{
    struct glyph
    {
        /// Unicode codepoint (!= utf-8 symbol)
        char_t codepoint = 0;
        /// spacing to next character
        float xadvance = 0.0f;
        /// geometry (relative to 0)
        math::vec2 xy0 = {0.0f, 0.0f};
        math::vec2 xy1 = {0.0f, 0.0f};
        /// texture coordinates
        math::vec2 uv0 = {0.0f, 0.0f};
        math::vec2 uv1 = {0.0f, 0.0f};
    };

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
        if(codepoint1 < kernings.size())
        {
            const auto& kern_values = kernings[codepoint1];
            if(codepoint2 < kern_values.size())
            {
                auto kerning = kern_values[codepoint2];
                return kerning;
            }
        }

        return 0.0f;
    }

    /// name of font
    std::string face_name;

    /// (point) size based on which the glyphs will be rasterized onto the texture
    float size = 0;

    /// spread of signed distance field (0 if no distance field is applied)
    int sdf_spread = 0;

    /// number of texture pixels per unit of geometry
    /// both horizontal and vertical
    /// this, of course, relies on several assumptions
    /// (which hold true for the current loaders)
    /// but may not necessarily do so for others
    /// 1. horizontal and vertical density ARE the same
    /// 2. all characters have the same pixel density
    float pixel_density = 0;

    /// height of a single line
    float line_height = 0;
    /// ascent of letters
    float ascent = 0;
    /// descent of letters
    float descent = 0;

    /// all loaded glyphs
    std::vector<glyph> glyphs;

    /// sparse vector of indices per codepoint
    std::vector<int> glyph_index;

    /// kerning lookup table per codepoint
    kerning_table_t kernings{};

    /// glyph to be used when requesting a non-existent one
    glyph fallback_glyph;

    /// rasterized font goes here
    surface_ptr surface;
};

using font_info_ptr = std::shared_ptr<font_info>;

}

