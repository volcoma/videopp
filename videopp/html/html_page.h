#pragma once
#include "html_container.h"

namespace video_ctrl
{

struct html_context
{
    html_context(renderer& r, const html_defaults& options);

    litehtml::context ctx;
    html_container container;
    renderer& rend;
};

class html_page
{
public:
    html_page(html_context& ctx);

    void present(const rect& dst_rect);
    void load(const std::string& html);

private:

    html_context& ctx_;
    litehtml::document::ptr document_{};
    int width{};
};

}
