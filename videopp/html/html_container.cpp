#include <utility>

#include "html_container.h"
#include "uri/uri.h"
namespace video_ctrl
{

litehtml::tstring urljoin(const litehtml::tstring& base_url, const litehtml::tstring& relative)
{
	try
	{
		Web::URI uri_base(base_url);
		Web::URI uri_res(uri_base, relative);
		return uri_res.toString();
	}
	catch(...)
	{
		return relative;
	}
}

void html_container::make_url(const litehtml::tchar_t* url, const litehtml::tchar_t* basepath,
							  litehtml::tstring& out)
{
	if(!basepath || (basepath && !basepath[0]))
	{
		if(!base_url_.empty())
		{
			out = urljoin(base_url_, std::string(url));
		}
		else
		{
			out = url;
		}
	}
	else
	{
		out = urljoin(std::string(basepath), std::string(url));
    }
}

rect html_container::get_clip()
{
    rect clip{};
    if(!clip_rects_.empty())
    {
        clip = clip_rects_.back();
    }

    return clip;
}

html_container::html_container(html_context& ctx)
	: ctx_(ctx)
{
	static size_t ids = 0;
	id_ = ++ids;
}

void html_container::present()
{
	ctx_.rend.draw_cmd_list(list_);
}

void html_container::invalidate()
{
	list_ = {};
}

void html_container::set_url(const std::string& url)
{
	url_ = url;
	base_url_ = url;
}

litehtml::uint_ptr html_container::create_font(const litehtml::tchar_t* face_name, int size, int weight,
											   litehtml::font_style style, unsigned int decoration,
											   litehtml::font_metrics* fm)
{
	std::string fnt_name = "serif";

	litehtml::string_vector fonts;
	litehtml::split_string(face_name, fonts, _t(","));


	bool italic = style == litehtml::fontStyleItalic || style == litehtml::fontStyleOblique;
	auto extract_font_metrics = [&](const html_font* fnt, litehtml::font_metrics* fm) {
		if(fnt && fm)
		{
			const auto& font = fnt->face;
			const auto& x_glyph = font->get_glyph('x');
			auto scale = fnt->scale;

			fm->ascent = static_cast<int>(font->ascent * scale);
			fm->descent = static_cast<int>(-font->descent * scale);
			fm->height = static_cast<int>(font->line_height * scale);
			fm->x_height = static_cast<int>((x_glyph.y1 - x_glyph.y0) * scale);
			fm->draw_spaces = italic || decoration;
		}
	};



	html_font_ptr font{};
	for(auto& family : fonts)
	{
		litehtml::trim(family);

		fnt_name = family;
		if(fnt_name.front() == '"')
		{
			fnt_name.erase(0, 1);
		}
		if(fnt_name.back() == '"')
		{
			fnt_name.pop_back();
		}
		font = ctx_.get_font(id_, fnt_name, size, weight, italic, decoration);

		if(font)
		{
            break;
        }
	}
	if(!font)
	{
        font = ctx_.get_font(id_, get_default_font_name(), size, weight, italic, decoration);
	}

	if(!font)
	{
        font = ctx_.get_font(id_, "embedded", size, weight, italic, decoration);
	}

    extract_font_metrics(font.get(), fm);
	return reinterpret_cast<litehtml::uint_ptr>(font.get());
}

void html_container::delete_font(litehtml::uint_ptr hFont)
{
	auto font_data = reinterpret_cast<html_font*>(hFont);
	if(!font_data)
	{
		return;
	}

	ctx_.delete_font(font_data);
}

int html_container::text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont)
{
	auto font_data = reinterpret_cast<html_font*>(hFont);

	if(!font_data)
	{
		return 0;
	}

	const auto& font = font_data->face;
	auto scale = font_data->scale;
	auto boldness = font_data->boldness;
	auto leaning = font_data->leaning;

	video_ctrl::text t;
	t.set_font(font);
	t.set_utf8_text(text);

	if(boldness > 0.0f)
	{
		t.set_outline_width(boldness);
	}
	if(leaning > 0.0f)
	{
        t.set_leaning(leaning);
	}

	return static_cast<int>(t.get_width() * scale);
}

void html_container::draw_text(litehtml::uint_ptr, const litehtml::tchar_t* text, litehtml::uint_ptr hFont,
							   litehtml::web_color color, const litehtml::position& pos, const litehtml::text_shadow& shadow)
{
	auto font_data = reinterpret_cast<html_font*>(hFont);

	if(!font_data)
	{
		return;
	}

	const auto& font = font_data->face;
	auto scale = font_data->scale;
	auto boldness = font_data->boldness;
	auto leaning = font_data->leaning;
	auto underline = font_data->underline;
	auto overline = font_data->overline;
	auto linethrough = font_data->linethrough;

	video_ctrl::color col = {color.red, color.green, color.blue, color.alpha};
	video_ctrl::text t;
	t.set_font(font);
	t.set_utf8_text(text);
	t.set_alignment(text::alignment::top_left);
	t.set_color(col);
	if(boldness > 0.0f)
	{
		t.set_outline_color(col);
		t.set_outline_width(boldness);
	}
	if(leaning > 0.0f)
	{
        t.set_leaning(leaning);
	}

	video_ctrl::color shadow_col = {shadow.color.red, shadow.color.green, shadow.color.blue, shadow.color.alpha};
	t.set_shadow_offsets({shadow.h_shadow, shadow.v_shadow});
	t.set_shadow_color(shadow_col);

	math::transformf transform;
	transform.set_scale(scale, scale, 1.0f);
	transform.set_position(pos.x, pos.y, 0.0f);

	list_.push_clip(get_clip());
	list_.add_text(t, transform);
    list_.pop_clip();

    auto line_width = scale * font->size / float(get_default_font_size());
	if(underline)
	{
        const auto& rect = t.get_rect();
        const auto& lines = t.get_descent_lines();
        for(const auto& line : lines)
        {
            auto v1 = transform.transform_coord({rect.x, line});
            auto v2 = transform.transform_coord({rect.x + rect.w, line});

            list_.add_line(v1, v2, col, line_width);
        }
	}

	if(overline)
	{
        const auto& rect = t.get_rect();
        const auto& lines = t.get_ascent_lines();
        for(const auto& line : lines)
        {
            auto v1 = transform.transform_coord({rect.x, line});
            auto v2 = transform.transform_coord({rect.x + rect.w, line});

            list_.add_line(v1, v2, col, line_width);
        }
	}

	if(linethrough)
	{
        const auto& g = font->get_glyph('x');
        float half_x_height = (g.y1 - g.y0) * 0.5f;
        const auto& rect = t.get_rect();
        const auto& lines = t.get_baseline_lines();
        for(const auto& line : lines)
        {
            auto v1 = transform.transform_coord({rect.x, line - half_x_height});
            auto v2 = transform.transform_coord({rect.x + rect.w, line - half_x_height});

            list_.add_line(v1, v2, col, line_width);
        }
	}
}

int html_container::pt_to_px(int pt)
{
	return static_cast<int>(round(pt * 125.0 / 72.0));
}

int html_container::get_default_font_size() const
{
	return ctx_.options.default_font_size;
}

const litehtml::tchar_t* html_container::get_default_font_name() const
{
	return ctx_.options.default_font.c_str();
}

void html_container::draw_list_marker(litehtml::uint_ptr /*hdc*/, const litehtml::list_marker& marker)
{
    list_.push_clip(get_clip());

	if(!marker.image.empty())
	{
		std::string url;
		make_url(marker.image.c_str(), marker.baseurl, url);

		auto image = ctx_.get_image(url);
		if(image)
		{
			rect src = image->get_rect();
			rect dst = {marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height};
			list_.add_image(image, src, dst);
		}
	}
	else
	{
		switch(marker.marker_type)
		{
			case litehtml::list_style_type_circle:
			{
				// draw_ellipse((cairo_t*)hdc, marker.pos.x, marker.pos.y, marker.pos.width,
				// marker.pos.height, 			 marker.color, 0.5);
				color col{marker.color.red, marker.color.green, marker.color.blue, marker.color.alpha};
				rect fill_rect = {marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height};

				list_.add_rect(fill_rect, col);
			}
			break;
			case litehtml::list_style_type_disc:
			{
				// fill_ellipse((cairo_t*)hdc, marker.pos.x, marker.pos.y, marker.pos.width,
				// marker.pos.height, 			 marker.color);
				color col{marker.color.red, marker.color.green, marker.color.blue, marker.color.alpha};
				rect fill_rect = {marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height};

				list_.add_rect(fill_rect, col);
			}
			break;
			case litehtml::list_style_type_square:
			{
				color col{marker.color.red, marker.color.green, marker.color.blue, marker.color.alpha};
				rect fill_rect = {marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height};

				list_.add_rect(fill_rect, col);
			}
			break;
			default:
			{
				color col{marker.color.red, marker.color.green, marker.color.blue, marker.color.alpha};
				rect fill_rect = {marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height};

				list_.add_rect(fill_rect, col);
			}
			break;
		}
	}

    list_.pop_clip();
}

void html_container::load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl,
								bool /*redraw_on_ready*/)
{
	std::string url;
	make_url(src, baseurl, url);

	ctx_.get_image(url);
}

void html_container::get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl,
									litehtml::size& sz)
{
	std::string url;
	make_url(src, baseurl, url);

	auto image = ctx_.get_image(url);
	if(image)
	{
		const auto& rect = image->get_rect();
		sz.width = rect.w;
		sz.height = rect.h;
	}
	else
	{
		sz.width = 0;
		sz.height = 0;
	}
}

void html_container::draw_background(litehtml::uint_ptr, const litehtml::background_paint& bg)
{
    list_.push_clip(get_clip());

	if(bg.image.empty())
	{
		color col{bg.color.red, bg.color.green, bg.color.blue, bg.color.alpha};
		rect fill_rect = {bg.clip_box.x, bg.clip_box.y, bg.clip_box.width, bg.clip_box.height};
		if(fill_rect)
		{
			list_.add_rect(fill_rect, col);
		}
	}
	else
	{
		std::string url;
		make_url(bg.image.c_str(), bg.baseurl.c_str(), url);

		auto image = ctx_.get_image(url);

		if(image)
		{
			rect src = image->get_rect();
			rect dst = {bg.clip_box.x, bg.clip_box.y, bg.image_size.width, bg.image_size.height};
			list_.add_image(image, src, dst);
		}
	}

	list_.pop_clip();
}

void html_container::draw_borders(litehtml::uint_ptr /*hdc*/, const litehtml::borders& borders,
								  const litehtml::position& draw_pos, bool /*root*/)
{
    list_.push_clip(get_clip());

    int bdr_top		= 0;
	int bdr_bottom	= 0;
	int bdr_left	= 0;
	int bdr_right	= 0;

	if(borders.top.width != 0 && borders.top.style > litehtml::border_style_hidden)
	{
		bdr_top = borders.top.width;
	}
	if(borders.bottom.width != 0 && borders.bottom.style > litehtml::border_style_hidden)
	{
		bdr_bottom = borders.bottom.width;
	}
	if(borders.left.width != 0 && borders.left.style > litehtml::border_style_hidden)
	{
		bdr_left = borders.left.width;
	}
	if(borders.right.width != 0 && borders.right.style > litehtml::border_style_hidden)
	{
		bdr_right = borders.right.width;
	}

	// draw right border
	if (bdr_right)
	{
        color col{borders.right.color.red, borders.right.color.green, borders.right.color.blue,
				  borders.right.color.alpha};
        list_.add_rect({draw_pos.right() - bdr_right, draw_pos.top(), bdr_right, draw_pos.height}, col);
	}

	// draw bottom border
	if(bdr_bottom)
	{
        color col{borders.bottom.color.red, borders.bottom.color.green, borders.bottom.color.blue,
				  borders.bottom.color.alpha};
        list_.add_rect({draw_pos.left(), draw_pos.bottom() - bdr_bottom, draw_pos.width, bdr_bottom}, col);
	}

	// draw top border
	if(bdr_top)
	{
		color col{borders.top.color.red, borders.top.color.green, borders.top.color.blue,
				  borders.top.color.alpha};
        list_.add_rect({draw_pos.left(), draw_pos.top(), draw_pos.width, bdr_top}, col);
	}

	// draw left border
	if (bdr_left)
	{
		color col{borders.left.color.red, borders.left.color.green, borders.left.color.blue,
				  borders.left.color.alpha};
        list_.add_rect({draw_pos.left(), draw_pos.top(), bdr_left, draw_pos.height}, col);
	}

	list_.pop_clip();
}

void html_container::set_caption(const litehtml::tchar_t*)
{
}

void html_container::set_base_url(const litehtml::tchar_t* base_url)
{
	if(base_url)
	{
		base_url_ = urljoin(url_, std::string(base_url));
	}
	else
	{
		base_url_ = url_;
	}
}

void html_container::link(const std::shared_ptr<litehtml::document>&, const litehtml::element::ptr&)
{
}

void html_container::on_anchor_click(const litehtml::tchar_t*, const litehtml::element::ptr&)
{
}

void html_container::set_cursor(const litehtml::tchar_t*)
{
}

void html_container::transform_text(litehtml::tstring&, litehtml::text_transform)
{
}

void html_container::import_css(litehtml::tstring& text, const litehtml::tstring& url,
								litehtml::tstring& baseurl)
{
	std::string css_url;
	make_url(url.c_str(), baseurl.c_str(), css_url);
	ctx_.load_file(css_url, text);

	if(!text.empty())
	{
		baseurl = css_url;
	}
}

void html_container::set_clip(const litehtml::position& pos, const litehtml::border_radiuses& /*bdr_radius*/,
							  bool valid_x, bool valid_y)
{
	rect clip_rect = {pos.x, pos.y, pos.width, pos.height};
	litehtml::position client_pos;
	get_client_rect(client_pos);
	if(!valid_x)
	{
		clip_rect.x = client_pos.x;
		clip_rect.w = client_pos.width;
	}
	if(!valid_y)
	{
		clip_rect.y = client_pos.y;
		clip_rect.h = client_pos.height;
	}
	clip_rects_.emplace_back(clip_rect);
}

void html_container::del_clip()
{
	if(!clip_rects_.empty())
	{
		clip_rects_.pop_back();
	}
}

void html_container::get_client_rect(litehtml::position& client) const
{
	const auto& r = ctx_.rend.get_rect();
	client.x = r.x;
	client.y = r.y;
	client.width = r.w;
	client.height = r.h;
}

std::shared_ptr<litehtml::element> html_container::create_element(const litehtml::tchar_t*,
																  const litehtml::string_map&,
																  const std::shared_ptr<litehtml::document>&)
{
	return {};
}

void html_container::get_media_features(litehtml::media_features& media) const
{
	litehtml::position client;
	get_client_rect(client);
	media.type = litehtml::media_type_screen;
	media.width = client.width;
	media.height = client.height;
	media.device_width = ctx_.rend.get_rect().w;
	media.device_height = ctx_.rend.get_rect().h;
	media.color = 8;
	media.monochrome = 0;
	media.color_index = 256;
	media.resolution = 96;
}

void html_container::get_language(litehtml::tstring& language, litehtml::tstring& culture) const
{
	language = _t("en");
	culture = _t("");
}
}
