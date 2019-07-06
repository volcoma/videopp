#include "html_page.h"

namespace video_ctrl
{

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

void html_page::load_from_file(const std::string& url)
{
	std::string html;
	if(ctx_.load_file(url, html))
	{
		load_from_utf8(html, get_path(url));
	}
}

void html_page::load_from_utf8(const std::string& html, const std::string& cwd)
{
	container_.invalidate();
	container_.set_url(cwd);
	document_ = litehtml::document::create_from_utf8(html.c_str(), &container_, &ctx_.ctx);

	posx = -1;
	posy = -1;
	width = -1;
}
}
