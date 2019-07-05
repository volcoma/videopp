#include <utility>

#include "html_container.h"

namespace video_ctrl
{

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

litehtml::uint_ptr html_container::create_font(const litehtml::tchar_t* face_name, int size, int weight,
											   litehtml::font_style style, unsigned int decoration,
											   litehtml::font_metrics* fm)
{
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
			fm->draw_spaces = style == litehtml::fontStyleItalic || decoration;
		}
	};

	auto font = ctx_.get_font(id_, face_name, size, weight);
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

	video_ctrl::text t;
	t.set_font(font);
	t.set_utf8_text(text);

	if(boldness > 0.0f)
	{
		t.set_outline_width(boldness);
	}

	return static_cast<int>(t.get_width() * scale);
}

void html_container::draw_text(litehtml::uint_ptr, const litehtml::tchar_t* text, litehtml::uint_ptr hFont,
							   litehtml::web_color color, const litehtml::position& pos)
{
	auto font_data = reinterpret_cast<html_font*>(hFont);

	if(!font_data)
	{
		return;
	}

	const auto& font = font_data->face;
	auto scale = font_data->scale;
	auto boldness = font_data->boldness;

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
	math::transformf transform;
	transform.set_scale(scale, scale, 1.0f);
	transform.set_position(pos.x, pos.y, 0.0f);
	list_.add_text(t, transform);
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
	if(!marker.image.empty())
	{
		/*litehtml::tstring url;
		make_url(marker.image.c_str(), marker.baseurl, url);

		lock_images_cache();
		images_map::iterator img_i = m_images.find(url.c_str());
		if(img_i != m_images.end())
		{
			if(img_i->second)
			{
				draw_txdib((cairo_t*) hdc, img_i->second, marker.pos.x, marker.pos.y, marker.pos.width,
		marker.pos.height);
			}
		}
		unlock_images_cache();*/
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
				break;
		}
	}
}

void html_container::load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* /*baseurl*/,
								bool /*redraw_on_ready*/)
{
	ctx_.get_image(src);
}

void html_container::get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* /*baseurl*/,
									litehtml::size& sz)
{
	auto image = ctx_.get_image(src);
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

		auto image = ctx_.get_image(bg.image);

		if(image)
		{
			rect src = image->get_rect();
			rect dst = {bg.clip_box.x, bg.clip_box.y, bg.image_size.width, bg.image_size.height};
			list_.add_image(image, src, dst);
		}
	}
}

void html_container::draw_borders(litehtml::uint_ptr /*hdc*/, const litehtml::borders& borders,
								  const litehtml::position& draw_pos, bool /*root*/)
{
	if(borders.top.width != 0 && borders.top.style > litehtml::border_style_hidden)
	{
		rect hollow_rect = {draw_pos.x, draw_pos.y, draw_pos.width, draw_pos.height};
		color col{borders.top.color.red, borders.top.color.green, borders.top.color.blue,
				  borders.top.color.alpha};

		if(hollow_rect)
		{
			list_.add_rect(hollow_rect, col, false, 1.0f);
		}
	}
}

void html_container::set_caption(const litehtml::tchar_t*)
{
}

void html_container::set_base_url(const litehtml::tchar_t*)
{
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
    ctx_.load_file(path + "/" + url, text);
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
