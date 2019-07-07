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
    void load_from_utf8(const std::string& html, const std::string& url = {});
    void load_from_file(const std::string& url);

private:

    html_context& ctx_;
    html_container container_;
    litehtml::document::ptr document_{};

    int x_{-1};
    int y_{-1};
    int width_{-1};
};

}
