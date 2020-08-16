#pragma once

#include "glyph_range.h"

#include "font_info.h"
#include <map>

namespace gfx
{

struct font_desc
{
    glyphs codepoint_ranges{};
    float font_size{};
    bool kerning{};
};

struct font_desc_memory_base85
{
    const char* data{};
    font_desc desc{};
};

struct font_desc_memory
{
    const void* data{};
    size_t data_size{};
    font_desc desc{};
};

struct font_desc_file
{
    std::string path{};
    font_desc desc{};
};

using font_file_descriptors = std::vector<font_desc_file>;
using font_memory_descriptors = std::vector<font_desc_memory>;
using font_memory_base85_descriptors = std::vector<font_desc_memory_base85>;
using font_weights = std::map<std::string, font_file_descriptors>;

font_info create_font_from_ttf_memory_compressed_base85(
    const font_memory_base85_descriptors& descs,
    const std::string& face_name = {}
    );

font_info create_font_from_ttf_memory_compressed(
    const font_memory_descriptors& descs,
    const std::string& face_name = {}
    );

font_info create_font_from_ttf_memory(
    const font_memory_descriptors& descs,
    const std::string& face_name = {}
    );

font_info create_font_from_ttf(
    const font_file_descriptors& descs,
    const std::string& face_name = {},
    bool auto_fit = false
    );

font_info create_font_from_ttf(
    const std::string& path, // font path
    const glyphs& codepoint_ranges, // ranges to rasterize
    float font_size // size to rasterize
);

font_info create_default_font(
    float font_size = 13 // size to rasterize
);

font_weights create_descriptions(const std::string& dir,
                                 const std::string& font_name = "NotoSans",
                                 bool cjk = false);

}
