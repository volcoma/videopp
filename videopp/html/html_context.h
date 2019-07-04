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
    int default_font_size{};
};
struct html_context
{
    html_context(renderer& r, html_defaults opts);

    struct font_face
    {
        font_ptr font;
        std::string key;

        // data to be used during drawing
        float scale{1.0f};
        float boldness{0.0f};
    };

    using font_face_ptr = std::shared_ptr<font_face>;
    font_face_ptr create_font(const std::string& face_name, int size, int weight);

    litehtml::context ctx;
    renderer& rend;
    html_defaults options;

    std::map<std::string, font_face_ptr> fonts_faces;
    std::map<std::string, font_ptr> fonts;
    std::map<std::string, video_ctrl::rect> textures;
};

}
