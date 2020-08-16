#pragma once

#include "surface.h"
#include "logger.h"
#include <fontpp/font.h>

#include <cstdint>
#include <vector>
#include <locale>
#include <codecvt>
#include <sstream>
#include <iomanip>

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

    std::string get_info() const
    {
        auto glyphs_mem_bytes = glyphs.size() * sizeof(glyph) + glyph_index.size() * sizeof(char_t);
        auto glyphs_mem_mb =  float(glyphs_mem_bytes) / float(1024 * 1024);
        std::stringstream ss{};
        ss << "\n";
        ss << "face       : " << face_name << "\n";
        ss << "size       : " << size << "\n";
        ss << "glyphs     : " << glyphs.size() << "\n";
        ss << "kerning    : " << kernings.size() << " pairs\n";
        ss << "glyphs mem : " << glyphs_mem_bytes << "b (" << std::setprecision(2) << glyphs_mem_mb << "mb)\n";
        if(surface)
        {
            auto atlas_mem_bytes =  surface->get_width() * surface->get_height();
            auto atlas_mem_mb =  float(atlas_mem_bytes) / float(1024 * 1024);

            ss << "atlas      : " << surface->get_width() << "x" << surface->get_height() << "\n";
            ss << "atlas mem  : " << atlas_mem_bytes << "b (" << std::setprecision(3) << atlas_mem_mb << "mb)\n";
            ss << "total mem  : " << glyphs_mem_bytes + atlas_mem_bytes << "b (" << std::setprecision(3) << glyphs_mem_mb + atlas_mem_mb << "mb)\n";
        }
        ss << "build time : " << build_time.count() << " ms\n";
        ss << "sdf time   : " << sdf_time.count() << " ms\n";
        ss << "total time : " << (build_time + sdf_time).count() << " ms\n";
        return ss.str();
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

    std::chrono::milliseconds build_time{};
    std::chrono::milliseconds sdf_time{};
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
    /// height of capital letters H, I
    float cap_height = 0;
    /// spread of signed distance field (0 if no distance field is applied)
    uint32_t sdf_spread = 0;

    bool pixel_snap{};
};

using font_info_ptr = std::shared_ptr<font_info>;

}

