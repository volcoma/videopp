#include "html_page.h"
#include "../logger.h"
#include <chrono>
namespace video_ctrl
{

struct bench
{
    using clock_t = std::chrono::high_resolution_clock;

    bench(const char* scope = "unknown")
        : name(scope)
    {}

    ~bench()
    {
        auto end = clock_t::now();
        auto dur = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start);
        log(std::string(name) + " took " + std::to_string(dur.count()) + "ms");
    }

    const char* name;
    clock_t::time_point start = clock_t::now();
};


std::string get_path(const std::string& str)
{
	size_t found = 0;
	found = str.find_last_of("/\\");
	if(found != std::string::npos)
	{
		return str.substr(0, found + 1);
	}

    return {};
}

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

	if(width_ != max_width)
	{
        bench mark("document::prepare_layout");
		document_->render(max_width);
	}

	if(x_ != x || y_ != y || width_ != max_width)
	{
	    bench mark("document::prepare_draw_cmds");
		container_.invalidate();
		document_->draw({}, x, y, nullptr);
	}

	x_ = x;
	y_ = y;
	width_ = max_width;

    //bench mark("document::present");
	container_.present();
}

void html_page::load_from_file(const std::string& url)
{
    bench mark("html_page::load_from_file");
	std::string html;
	if(ctx_.load_file(url, html))
	{
		load_from_utf8(html, get_path(url));
	}
}

void html_page::load_from_utf8(const std::string& html, const std::string& url)
{
    bench mark("html_page::load_from_utf8");
	container_.invalidate();
	container_.set_url(url);

	document_ = litehtml::document::create_from_utf8(html.c_str(), &container_, &ctx_.ctx);

	x_ = -1;
	y_ = -1;
	width_ = -1;
}
}
