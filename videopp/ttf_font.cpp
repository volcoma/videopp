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
                                       fnt::font_config*,
                                       const T&,
                                       const fnt::font_wchar*)>& add_to_atlas, bool log_info = true)
{
    fnt::font_atlas atlas{};
    atlas.max_texture_size = 1024 * 8;
//    atlas.flags |= fnt::font_atlas_flags::no_power_of_two_height;

    constexpr bool vectorize = true;
    float sdf_spread = vectorize ? std::round(0.1f * descs.front().desc.font_size) : 0.0f;
    atlas.sdf_spread = uint32_t(sdf_spread);

    font_info f;
    f.face_name = fontname(face_name.c_str());

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
        cfg.merge_mode = i > 0;
        cfg.kerning_glyphs_limit = desc.desc.kerning ? 512 : 0;
        cfg.pixel_snap_h = true;
        auto font = add_to_atlas(atlas, &cfg, desc, ranges[i].data());
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
    f.build_time = atlas.build_time;
    f.sdf_time = atlas.sdf_time;

    if(log_info)
    {
        log(f.get_info());
    }

    return f;
}


template<typename T>
font_info create_font_from_description_auto_fit_greedy(const std::vector<T>& descs_in,
                                                       const std::string& face_name,
                                                       const std::function<fnt::font_info*(
                                                       fnt::font_atlas&,
                                                       fnt::font_config*,
                                                       const T&,
                                                       const fnt::font_wchar*)>& add_to_atlas)
{

    float upper_bound{};
    float lower_bound{};

    for(const auto& desc : descs_in)
    {
        upper_bound = std::max(upper_bound, desc.desc.font_size);
    }

    std::vector<float> sizes{};
    while(true)
    {
        auto descs = descs_in;

        for(auto& desc : descs)
        {
            desc.desc.font_size = upper_bound;
        }

        try
        {
            sizes.emplace_back(upper_bound);
            auto f = create_font_from_description<T>(descs, face_name, add_to_atlas, false);
            if(upper_bound - lower_bound < 2.0f || sizes.size() == 1)
            {
                std::stringstream ss{};
                ss << "probes : [";
                for(size_t i = 0; i < sizes.size(); ++i)
                {
                    ss << sizes[i];
                    if(i != sizes.size() - 1)
                    {
                        ss << ",";
                    }
                }
                ss << "]\n" << f.get_info();
                log(ss.str());
                return f;
            }

            auto sz = upper_bound;
            upper_bound += math::round((upper_bound - lower_bound) / 2.0f);
            lower_bound = sz;
        }
        catch (...)
        {
            upper_bound -= math::round((upper_bound - lower_bound) / 2.0f);
        }
    }

    throw std::runtime_error("[" + face_name + "] - could not load font.");
}

}

font_info create_font_from_ttf(const std::string& path, const glyphs& codepoint_ranges, float font_size)
{
    std::vector<font_desc_file> descs{};
    descs.emplace_back();
    auto& desc = descs.back();
    desc.path = path;
    desc.desc.codepoint_ranges = codepoint_ranges;
    desc.desc.font_size = font_size;

    return create_font_from_ttf(descs, path);
}

font_info create_default_font(float font_size)
{
    fnt::font_atlas atlas{};
    atlas.max_texture_size = 1024 * 8;
    //    atlas.flags |= fnt::font_atlas_flags::no_power_of_two_height;

    float sdf_spread = std::round(0.1f * font_size);
    atlas.sdf_spread = uint32_t(sdf_spread);

    fnt::font_config cfg{};
    cfg.size_pixels = font_size;
    cfg.pixel_snap_h = true;
    auto font = atlas.add_font_default(&cfg);

    return create_font("default", atlas, font);
}


font_info create_font_from_ttf(const std::vector<font_desc_file>& descs,
                               const std::string& face_name,
                               bool auto_fit)
{
    auto add_to_atlas = [](
            fnt::font_atlas& atlas,
            fnt::font_config* cfg,
            const font_desc_file& desc,
            const fnt::font_wchar* glyph_ranges)
    {
        return atlas.add_font_from_file_ttf(desc.path.c_str(), desc.desc.font_size, cfg, glyph_ranges);
    };


    auto fname = !face_name.empty() ? face_name : descs.front().path;

    if(auto_fit)
    {
        return create_font_from_description_auto_fit_greedy<font_desc_file>(descs, fname, add_to_atlas);
    }

    return create_font_from_description<font_desc_file>(descs, fname, add_to_atlas);
}


font_info create_font_from_ttf_memory_compressed_base85(const std::vector<font_desc_memory_base85>& descs,
                                             const std::string& face_name)
{
    auto add_to_atlas = [](
            fnt::font_atlas& atlas,
            fnt::font_config* cfg,
            const font_desc_memory_base85& desc,
            const fnt::font_wchar* glyph_ranges)
    {
        cfg->font_data_owned_by_atlas = false;
        return atlas.add_font_from_memory_compressed_base85_ttf(desc.data, desc.desc.font_size, cfg, glyph_ranges);
    };

    return create_font_from_description<font_desc_memory_base85>(descs, face_name, add_to_atlas);

}

font_info create_font_from_ttf_memory_compressed(const std::vector<font_desc_memory>& descs,
                                                 const std::string& face_name)
{
    auto add_to_atlas = [](
            fnt::font_atlas& atlas,
            fnt::font_config* cfg,
            const font_desc_memory& desc,
            const fnt::font_wchar* glyph_ranges)
    {
        cfg->font_data_owned_by_atlas = false;
        return atlas.add_font_from_memory_compressed_ttf((void*)desc.data, desc.data_size, desc.desc.font_size, cfg, glyph_ranges);
    };

    return create_font_from_description<font_desc_memory>(descs, face_name, add_to_atlas);

}

font_info create_font_from_ttf_memory(const std::vector<font_desc_memory>& descs,
                                      const std::string& face_name)
{

    auto add_to_atlas = [](
            fnt::font_atlas& atlas,
            fnt::font_config* cfg,
            const font_desc_memory& desc,
            const fnt::font_wchar* glyph_ranges)
    {
        cfg->font_data_owned_by_atlas = false;
        return atlas.add_font_from_memory_ttf((void*)desc.data, desc.data_size, desc.desc.font_size, cfg, glyph_ranges);
    };

    return create_font_from_description<font_desc_memory>(descs, face_name, add_to_atlas);

}

font_weights create_descriptions(const std::string& dir,
                                 const std::string& font_name,
                                 bool cjk)
{
    log("Create System Font Descriptions.");
    log("CJK included : " + std::string(cjk ? "true" : "false"));

    constexpr float font_size = 50.0f;

    font_weights fonts_to_load;
    for(auto type : {/*"Thin", "Light", "Medium", */"Regular", "Bold", "Black"})
    {
        auto& descs = fonts_to_load[type];
        {
            descs.emplace_back();
            auto& desc = descs.back();
            desc.path.append(dir)
                     .append("/")
                     .append(font_name)
                     .append("-")
                     .append(type)
                     .append(".ttf");
            glyphs_builder builder;
            builder.add(get_all_glyph_range());
            desc.desc.codepoint_ranges = builder.get();
            desc.desc.font_size = font_size;

        }
        {
            descs.emplace_back();
            auto& desc = descs.back();
            desc.path.append(dir)
                     .append("/")
                     .append(font_name)
                     .append("Thai-")
                     .append(type)
                     .append(".ttf");
            glyphs_builder builder;
            builder.add(get_thai_glyph_range());

            desc.desc.codepoint_ranges = builder.get();
            desc.desc.font_size = font_size;
        }
//        {
//            descs.emplace_back();
//            auto& desc = descs.back();
//            desc.path.append(dir)
//                     .append("/")
//                     .append(font_name)
//                     .append("Arabic-")
//                     .append(type)
//                     .append(".ttf");
//            gfx::glyphs_builder builder;
//            builder.add(gfx::get_arabic_glyph_range());
//            desc.desc.codepoint_ranges = builder.get();
//            desc.desc.font_size = font_size;
//        }

        if(cjk)
        {
            descs.emplace_back();
            auto& desc = descs.back();
            desc.path.append(dir)
                     .append("/")
                     .append(font_name)
                     .append("CJK-")
                     .append(type)
                     .append(".ttf");
            glyphs_builder builder;
            builder.add(get_chinese_glyph_range());
            builder.add(get_japanese_glyph_range());
//            builder.add(gfx::get_korean_glyph_range());

            desc.desc.codepoint_ranges = builder.get();
            desc.desc.font_size = font_size;
        }
    }

    {
        auto& descs = fonts_to_load["Mono"];
        {
            descs.emplace_back();
            auto& desc = descs.back();
            desc.path.append(dir)
                     .append("/")
                     .append(font_name)
                     .append("Mono-Regular")
                     .append(".ttf");
            glyphs_builder builder;
            builder.add(get_all_glyph_range());
            desc.desc.codepoint_ranges = builder.get();
            desc.desc.font_size = font_size;
        }
    }

    return fonts_to_load;
}

}
