#include <utility>

#include "html_page.h"
#include <master.css>

namespace video_ctrl
{


html_context::html_context(renderer& r, html_defaults opts)
    : rend(r)
    , options(std::move(opts))
{
    ctx.load_master_stylesheet(master_css);
}

html_context::font_face_ptr html_context::create_font(const std::string& face_name, int size, int weight)
{
    std::string key = face_name + std::to_string(size);
	auto it = fonts_faces.find(key);
	if(it != fonts_faces.end())
	{
		return it->second;
	}

	auto face = std::make_shared<font_face>();
	int bold_weight = 500;
	if(weight > bold_weight)
	{
		float step = (weight - bold_weight) / 100.0f;
		face->boldness = step * 0.05f;
	}
	face->key = key;

	constexpr int rasterize_size = 50;
	auto font_path = options.fonts_dir + "/" + face_name + ".ttf";

	auto find_it = fonts.find(font_path);
	if(find_it != std::end(fonts))
	{
		face->font = find_it->second;
	}
	else
	{
		glyphs_builder builder;
		builder.add(get_latin_glyph_range());
		builder.add(get_cyrillic_glyph_range());
		try
		{
			face->font = rend.create_font(create_font_from_ttf(font_path, builder.get(), rasterize_size, 2));
        }
		catch(...)
		{
            if(face_name != options.default_font)
            {
                return create_font(options.default_font.c_str(), size, weight);
            }
            else
            {
                face->font = rend.create_font(create_default_font(rasterize_size, 2));
            }
		}
		fonts[font_path] = face->font;
	}

	face->scale = size / float(face->font->size);

	fonts_faces[key] = face;

    return face;
}


html_page::html_page(html_context& ctx)
    : ctx_(ctx)
    , container_(ctx)

{
}

void html_page::draw(int x, int y, int max_width)
{
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

void html_page::load(const std::string& html)
{
    document_ = litehtml::document::create_from_utf8(html.c_str(), &container_, &ctx_.ctx);

    posx = -1;
    posy = -1;
    width = -1;
}


}
