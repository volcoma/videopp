#pragma once
#include "html_container.h"

namespace video_ctrl
{

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

class html_page
{
public:
    html_page(html_context& ctx);

    void draw(int x, int y, int max_width);
    void load(const std::string& html);

private:

    html_context& ctx_;
    html_container container_;
    litehtml::document::ptr document_{};

    int posx{-1};
    int posy{-1};
    int width{-1};
};

}
