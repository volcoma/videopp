#include "html_page.h"

namespace video_ctrl
{

html_page::html_page(html_context& ctx)
    : ctx_(ctx)
    , container_(ctx)

{
}

void html_page::draw(int x, int y, int max_width)
{
    if(!document_)
    {
        return;
    }

    if(width != max_width)
    {
        document_->render(max_width);
    }

    if(posx != x || posy != y || width != max_width)
    {
        container_.invalidate();
        document_->draw({}, x, y, nullptr);
    }

    posx = x;
    posy = y;
    width = max_width;

    container_.present();
}

void html_page::load(const std::string& html, const std::string& path)
{
    container_.invalidate();
    container_.path = path;
    document_ = litehtml::document::create_from_utf8(html.c_str(), &container_, &ctx_.ctx);

    posx = -1;
    posy = -1;
    width = -1;
}


}
