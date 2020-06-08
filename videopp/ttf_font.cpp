#include "ttf_font.h"

#include "logger.h"

#include <array>
#include <algorithm>
#include <memory>
#include <iostream>

#include <fontpp/font.h>

namespace gfx
{
namespace
{
void merge_maps(kerning_table_t& lhs, const kerning_table_t& rhs)
{
    auto lhs_it = std::begin(lhs);
    auto rhs_it = std::begin(rhs);

    while (lhs_it != std::end(lhs) && rhs_it != std::end(rhs))
    {
        /* If the rhs value is less than the lhs value, then insert it into the
           lhs map and skip past it. */
        if (rhs_it->first < lhs_it->first)
        {
            lhs.insert(lhs_it, *rhs_it); // Use lhs_it as a hint.
            ++rhs_it;
        }
        /* Otherwise, if the values are equal, overwrite the lhs value and move both
           iterators forward. */
        else if (rhs_it->first == lhs_it->first)
        {
            lhs_it->second = rhs_it->second;
            ++lhs_it; ++rhs_it;
        }
        /* Otherwise the rhs value is bigger, so skip past the lhs value. */
        else
        {
            ++lhs_it;
        }

    }

    /* At this point we've exhausted one of the two ranges.  Add what's left of the
       rhs values to the lhs map, since we know there are no duplicates there. */
    lhs.insert(rhs_it, std::end(rhs));
}

std::string fontname(const char* path)
{
    auto spos = 0;
    auto ppos = 0;
    int i = 0;
    for (char c = path[i]; c; c = path[++i])
    {
        if (c == '/') spos = i;
        if (c == '.') ppos = i;
    }

    if((spos == ppos) && ppos == 0)
    {
        return path;
    }
    if (ppos > spos)
    {
        return {path + spos + 1, path + ppos};
    }

    return {path + spos + 1, path + i};
}

font_info create_font(const std::string& id, fnt::font_atlas& atlas, fnt::font_info* font)
{
    if(!font)
    {
        throw std::runtime_error("[" + id + "] - Could not load.");
    }

    std::string err{};
    if(!atlas.build(err))
    {
        throw std::runtime_error("[" + id + "] - " + err);
    }

    font_info f;
    f.glyphs = std::move(font->glyphs);
    f.glyph_index = std::move(font->index_lookup);

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
    f.cap_height = font->cap_height;
    f.line_height = font->line_height;
    f.kernings = std::move(font->kernings);
    f.size = font->font_size;
    f.surface = std::make_unique<surface>(std::move(atlas.tex_pixels_alpha8), atlas.tex_width, atlas.tex_height, pix_type::gray);
    f.sdf_spread = atlas.sdf_spread;
    f.face_name = fontname(id.c_str());

    return f;
}

void add_to_font(font_info& f, fnt::font_info* font)
{
    f.glyphs.reserve(f.glyphs.size() + font->glyphs.size());
    std::copy(std::begin(font->glyphs), std::end(font->glyphs), std::back_inserter(f.glyphs));

    f.glyph_index.reserve(f.glyph_index.size() + font->index_lookup.size());
    std::copy(std::begin(font->index_lookup), std::end(font->index_lookup), std::back_inserter(f.glyph_index));

    merge_maps(f.kernings, font->kernings);

    if(!font->config_data->merge_mode)
    {
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
        f.cap_height = font->cap_height;
        f.line_height = font->line_height;
        f.size = font->font_size;
    }
}

template<typename T>
font_info create_font_from_description(const std::vector<T>& descs,
                                       const std::string& face_name,
                                       const std::function<fnt::font_info*(
                                       fnt::font_atlas&,
                                       const T&,
                                       const fnt::font_config*,
                                       const fnt::font_wchar*)>& add_to_atlas)
{
    fnt::font_atlas atlas{};
    atlas.max_texture_size = 1024 * 8;

    constexpr bool vectorize = true;
    float sdf_spread = vectorize ? std::round(0.1f * descs.front().desc.font_size) : 0.0f;
    atlas.sdf_spread = uint32_t(sdf_spread);

    font_info f;
    f.face_name = face_name;

    // these must be kept alive until atlas.build
    std::vector<std::vector<fnt::font_wchar>> ranges{};
    ranges.resize(descs.size());

    for(size_t i = 0; i < descs.size(); ++i)
    {
        const auto& desc = descs[i];

        if(desc.desc.codepoint_ranges.empty())
        {
            throw std::runtime_error("[" + face_name + "] - Empty range was supplied.");
        }

        fnt::font_glyph_ranges_builder builder{};

        for(const auto& cp_range : desc.desc.codepoint_ranges)
        {
            std::array<fnt::font_wchar, 3> range = {{fnt::font_wchar(cp_range.first), fnt::font_wchar(cp_range.second), 0}};
            builder.add_ranges(range.data());
        }
        ranges[i] = builder.build_ranges();

        fnt::font_config cfg{};
        if(i > 0)
        {
            cfg.merge_mode = true;
        }
        cfg.kerning_glyphs_limit = 0;//kerning ? 512 : 0;
        cfg.pixel_snap_h = true;
        auto font = add_to_atlas(atlas, desc, &cfg, ranges[i].data());
        if(!font)
        {
            throw std::runtime_error("[" + face_name + "] - Could not load.");
        }
    }

    std::string err{};
    if(!atlas.build(err))
    {
        throw std::runtime_error("[" + face_name + "] - " + err);
    }

    for(const auto& font : atlas.fonts)
    {
        add_to_font(f, font.get());
    }

    f.surface = std::make_unique<surface>(std::move(atlas.tex_pixels_alpha8), atlas.tex_width, atlas.tex_height, pix_type::gray);
    f.sdf_spread = atlas.sdf_spread;

    return f;
}

}

font_info create_font_from_ttf(const std::string& path, const glyphs& codepoint_ranges, float font_size)
{
    fnt::font_atlas atlas{};
    atlas.max_texture_size = 1024 * 8;

    constexpr bool vectorize = true;
    float sdf_spread = vectorize ? std::round(0.1f * font_size) : 0.0f;
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
    cfg.kerning_glyphs_limit = 0;//kerning ? 512 : 0;
    cfg.pixel_snap_h = true;
    auto font = atlas.add_font_from_file_ttf(path.c_str(), font_size, &cfg, ranges.data());
    return create_font(path, atlas, font);
}

font_info create_default_font(float font_size)
{
    fnt::font_atlas atlas{};
    atlas.max_texture_size = 1024 * 8;
    float sdf_spread = std::round(0.1f * font_size);
    atlas.sdf_spread = uint32_t(sdf_spread);

    fnt::font_config cfg{};
    cfg.size_pixels = font_size;
    cfg.pixel_snap_h = true;
    auto font = atlas.add_font_default(&cfg);

    return create_font("default", atlas, font);
}


font_info create_font_from_ttf(const std::vector<font_desc_file>& descs,
                               const std::string& face_name)
{
    auto add_to_atlas = [](
            fnt::font_atlas& atlas,
            const font_desc_file& desc,
            const fnt::font_config* cfg,
            const fnt::font_wchar* glyph_ranges)
    {
        return atlas.add_font_from_file_ttf(desc.path.c_str(), desc.desc.font_size, cfg, glyph_ranges);
    };

    auto fname = !face_name.empty() ? face_name : descs.front().path;

    return create_font_from_description<font_desc_file>(descs, fname, add_to_atlas);
}


font_info create_font_from_ttf_memory_compressed_base85(const std::vector<font_desc_memory_base85>& descs,
                                             const std::string& face_name)
{
    auto add_to_atlas = [](
            fnt::font_atlas& atlas,
            const font_desc_memory_base85& desc,
            const fnt::font_config* cfg,
            const fnt::font_wchar* glyph_ranges)
    {
        return atlas.add_font_from_memory_compressed_base85_ttf(desc.data, desc.desc.font_size, cfg, glyph_ranges);
    };

    return create_font_from_description<font_desc_memory_base85>(descs, face_name, add_to_atlas);

}

font_info create_font_from_ttf_memory_compressed(const std::vector<font_desc_memory>& descs,
                                                 const std::string& face_name)
{
    auto add_to_atlas = [](
            fnt::font_atlas& atlas,
            const font_desc_memory& desc,
            const fnt::font_config* cfg,
            const fnt::font_wchar* glyph_ranges)
    {
        return atlas.add_font_from_memory_compressed_ttf((void*)desc.data, desc.data_size, desc.desc.font_size, cfg, glyph_ranges);
    };

    return create_font_from_description<font_desc_memory>(descs, face_name, add_to_atlas);

}

font_info create_font_from_ttf_memory(const std::vector<font_desc_memory>& descs,
                                      const std::string& face_name)
{

    auto add_to_atlas = [](
            fnt::font_atlas& atlas,
            const font_desc_memory& desc,
            const fnt::font_config* cfg,
            const fnt::font_wchar* glyph_ranges)
    {
        return atlas.add_font_from_memory_ttf((void*)desc.data, desc.data_size, desc.desc.font_size, cfg, glyph_ranges);
    };

    return create_font_from_description<font_desc_memory>(descs, face_name, add_to_atlas);

}

}
