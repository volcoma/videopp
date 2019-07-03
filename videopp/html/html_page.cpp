#include "html_page.h"
#include <master.css>

namespace video_ctrl
{


html_context::html_context(renderer& r, const html_defaults& options)
    : container(r, options)
    , rend(r)
{
    ctx.load_master_stylesheet(master_css);
}


html_page::html_page(html_context& ctx)
    : ctx_(ctx)
{
}

void html_page::present(const rect& dst_rect)
{
    if(width != dst_rect.w)
    {
        width = dst_rect.w;
        document_->render(width);
    }

    litehtml::position clip_rect = {dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h};
    document_->draw({}, dst_rect.x, dst_rect.y, &clip_rect);

    ctx_.container.present();
    ctx_.container.invalidate_draw_list();
}

void html_page::load(const std::string& html)
{
    document_ = litehtml::document::createFromUTF8(html.c_str(), &ctx_.container, &ctx_.ctx);
    ctx_.container.invalidate_draw_list();
}


}
