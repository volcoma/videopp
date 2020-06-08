#pragma once

#include "glyph_range.h"

#include "font_info.h"

namespace gfx
{

struct font_desc
{
    glyphs codepoint_ranges{};
    float font_size{};
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

font_info create_font_from_ttf_memory_compressed_base85(
    const std::vector<font_desc_memory_base85>& descs,
    const std::string& face_name = {}
    );

font_info create_font_from_ttf_memory_compressed(
    const std::vector<font_desc_memory>& descs,
    const std::string& face_name = {}
    );

font_info create_font_from_ttf_memory(
    const std::vector<font_desc_memory>& descs,
    const std::string& face_name = {}
    );

font_info create_font_from_ttf(
    const std::vector<font_desc_file>& descs,
    const std::string& face_name = {}
    );

font_info create_font_from_ttf(
    const std::string& path, // font path
    const glyphs& codepoint_ranges, // ranges to rasterize
    float font_size // size to rasterize
);

font_info create_default_font(
    float font_size = 13 // size to rasterize
);

}
