#pragma once
#include "../color.h"
#include "../glyph_range.h"
#include "../rect.h"
#include "../renderer.h"
#include "../text.h"
#include "../ttf_font.h"

#include <litehtml.h>

namespace video_ctrl
{
struct html_defaults
{
    std::string fonts_dir{};
    std::string images_dir{};
    std::string default_font{};
    std::string default_monospace_font{};
    int default_font_size{};
};

struct html_font
{
    font_ptr face;
    std::string key;

    // data to be used during drawing
    float scale{1.0f};
    float boldness{0.0f};
};
using html_font_ptr = std::shared_ptr<html_font>;


struct html_context
{
    html_context(renderer& r, html_defaults opts);

    html_font_ptr get_font(size_t page_uid, const std::string& face_name, int size, int weight);
    void delete_font(html_font* font);

    texture_ptr get_image(const std::string& src);

    litehtml::context ctx;
    renderer& rend;
    html_defaults options;

    std::unordered_map<std::string, html_font_ptr> html_fonts;
    std::unordered_map<std::string, font_ptr> fonts;
    std::unordered_map<std::string, texture_ptr> images;
};

}
