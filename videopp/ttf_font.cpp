#include "ttf_font.h"

#include "logger.h"

#include <algorithm>
#include <memory>
#include <iostream>

#include <fontpp/font.h>

namespace gfx
{
namespace
{
font_info create_font(const std::string& id, fnt::font_atlas& atlas, fnt::font_info* font)
{
    if(!font)
    {
        throw std::runtime_error("[" + id + "] - Could not load.");
    }

    std::string err{};
    if(!atlas.build(fnt::font_rasterizer::freetype, err))
    {
        throw std::runtime_error("[" + id + "] - " + err);
    }

    font_info f;

    f.glyphs.reserve(font->glyphs.size());
    for(const auto& font_glyph : font->glyphs)
    {
        f.glyphs.emplace_back(font_glyph);
    }

    f.glyph_index.reserve(font->index_lookup.size());
    for(const auto& idx : font->index_lookup)
    {
        f.glyph_index.push_back(idx);
    }

    if(font->fallback_glyph)
    {
        const auto& font_glyph = *font->fallback_glyph;
        f.fallback_glyph = font_glyph;
    }
    else if(!f.glyphs.empty())
    {
        f.fallback_glyph = f.glyphs.front();
    }

    f.ascent = font->ascent;
    f.descent = font->descent;
    f.x_height = font->x_height;
    f.line_height = font->line_height;
    f.kernings = std::move(font->kernings);
    f.size = font->font_size;
    f.surface = std::make_unique<surface>(std::move(atlas.tex_pixels_alpha8), atlas.tex_width, atlas.tex_height, pix_type::red);
    f.sdf_spread = atlas.sdf_spread;
    f.face_name = id;

    return f;
}
}

font_info create_font_from_ttf(const std::string& path, const glyphs& codepoint_ranges, float font_size,
                               int sdf_spread /*= 0*/, bool kerning /*= false*/)
{
    fnt::font_atlas atlas{};
    atlas.max_texture_size = 1024 * 8;
    atlas.sdf_spread = uint32_t(sdf_spread);
    fnt::font_glyph_ranges_builder builder{};

    if(codepoint_ranges.empty())
    {
        throw std::runtime_error("[" + path + "] - Empty range was supplied.");
    }

    for(const auto& cp_range : codepoint_ranges)
    {
        std::array<fnt::font_wchar, 3> range = {{fnt::font_wchar(cp_range.first), fnt::font_wchar(cp_range.second), 0}};
        builder.add_ranges(range.data());
    }
    auto ranges = builder.build_ranges();

    fnt::font_config cfg{};
    cfg.kerning_glyphs_limit = kerning ? 512 : 0;
    cfg.pixel_snap_h = true;
    auto font = atlas.add_font_from_file_ttf(path.c_str(), font_size, &cfg, ranges.data());
    return create_font(path, atlas, font);
}

font_info create_default_font(float font_size, int sdf_spread)
{
    fnt::font_atlas atlas{};
    atlas.max_texture_size = 1024 * 8;
    atlas.sdf_spread = uint32_t(sdf_spread);

    fnt::font_config cfg{};
    cfg.size_pixels = font_size;
    cfg.pixel_snap_h = true;
    auto font = atlas.add_font_default(&cfg);

    return create_font("default", atlas, font);
}

}
