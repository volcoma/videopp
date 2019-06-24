#include "ttf_font.h"

#include "logger.h"

#include <algorithm>
#include <memory>
#include <iostream>

#include <fontpp/font.h>

namespace video_ctrl
{

font_info create_font_from_ttf(const std::string& path, const glyphs& codepoint_ranges, float font_size,
                               int sdf_spread /*= 0*/)
{

    font_info f;
    f.face_name = path;
    f.size = font_size;
    f.sdf_spread = sdf_spread;
    f.pixel_density = 1; // stbtt renders 1:1

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
        fnt::font_wchar range[] = {fnt::font_wchar(cp_range.first), fnt::font_wchar(cp_range.second), 0};
        builder.add_ranges(range);
    }
    auto ranges = builder.build_ranges();

    auto font = atlas.add_font_from_file_ttf(path.c_str(), font_size, nullptr, ranges.data());
    if(!font)
    {
        throw std::runtime_error("[" + path + "] - Could not load.");
    }

    std::string err{};
    if(!atlas.build(fnt::font_rasterizer::stb, err))
    {
        throw std::runtime_error("[" + path + "] - " + err);
    }

    auto create_glyph = [](const fnt::font_glyph& font_glyph)
    {
        font_info::glyph glyph;
        glyph.uv0 = {font_glyph.u0, font_glyph.v0};
        glyph.uv1 = {font_glyph.u1, font_glyph.v1};

        glyph.xy0 = {font_glyph.x0, font_glyph.y0};
        glyph.xy1 = {font_glyph.x1, font_glyph.y1};

        glyph.xadvance = font_glyph.advance_x;
        glyph.codepoint = font_glyph.codepoint;
        return glyph;
    };

    f.glyphs.reserve(font->glyphs.size());
    for(const auto& font_glyph : font->glyphs)
    {
        f.glyphs.emplace_back(create_glyph(font_glyph));
    }

    f.glyph_index.reserve(font->index_lookup.size());
    for(const auto& idx : font->index_lookup)
    {
        f.glyph_index.push_back(idx);
    }

    if(font->fallback_glyph)
    {
        const auto& font_glyph = *font->fallback_glyph;
        f.fallback_glyph = create_glyph(font_glyph);
    }
    else if(!f.glyphs.empty())
    {
        f.fallback_glyph = f.glyphs.front();
    }
    f.ascent = font->ascent;
    f.descent = font->descent;
    f.line_height = font->line_height;
    f.kernings = std::move(font->kernings);

    f.surface = std::make_unique<surface>(std::move(atlas.tex_pixels_alpha8), atlas.tex_width, atlas.tex_height, pix_type::gray);

    return f;
}

font_info create_default_font(float font_size, int sdf_spread)
{
    font_info f;
    f.face_name = "default";
    f.size = font_size;
    f.sdf_spread = sdf_spread;
    f.pixel_density = 1; // stbtt renders 1:1

    fnt::font_atlas atlas{};
    atlas.max_texture_size = 1024 * 8;
    atlas.sdf_spread = uint32_t(sdf_spread);
    fnt::font_glyph_ranges_builder builder{};
    fnt::font_config cfg{};
    cfg.size_pixels = font_size;
    auto font = atlas.add_font_default(&cfg);
    if(!font)
    {
        throw std::runtime_error("[default] - Could not load.");
    }

    std::string err{};
    if(!atlas.build(fnt::font_rasterizer::stb, err))
    {
        throw std::runtime_error("[default] - " + err);
    }

    auto create_glyph = [](const fnt::font_glyph& font_glyph)
    {
        font_info::glyph glyph;
        glyph.uv0 = {font_glyph.u0, font_glyph.v0};
        glyph.uv1 = {font_glyph.u1, font_glyph.v1};

        glyph.xy0 = {font_glyph.x0, font_glyph.y0};
        glyph.xy1 = {font_glyph.x1, font_glyph.y1};

        glyph.xadvance = font_glyph.advance_x;
        glyph.codepoint = font_glyph.codepoint;
        return glyph;
    };

    f.glyphs.reserve(font->glyphs.size());
    for(const auto& font_glyph : font->glyphs)
    {
        f.glyphs.emplace_back(create_glyph(font_glyph));
    }

    f.glyph_index.reserve(font->index_lookup.size());
    for(const auto& idx : font->index_lookup)
    {
        f.glyph_index.push_back(idx);
    }

    if(font->fallback_glyph)
    {
        const auto& font_glyph = *font->fallback_glyph;
        f.fallback_glyph = create_glyph(font_glyph);
    }
    else if(!f.glyphs.empty())
    {
        f.fallback_glyph = f.glyphs.front();
    }

    f.ascent = font->ascent;
    f.descent = font->descent;
    f.line_height = font->line_height;
    f.surface = std::make_unique<surface>(std::move(atlas.tex_pixels_alpha8), atlas.tex_width, atlas.tex_height, pix_type::gray);

    return f;
}

}
