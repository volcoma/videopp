#pragma once
#include "html_context.h"
#include "html_container.h"

namespace video_ctrl
{


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
