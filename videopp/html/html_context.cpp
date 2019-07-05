#include "html_context.h"
#include <master.css>

namespace video_ctrl
{


html_context::html_context(renderer& r, html_defaults opts)
    : rend(r)
    , options(std::move(opts))
{
    ctx.load_master_stylesheet(master_css);
}

void html_context::delete_font(html_font* /*font*/)
{
//    if(!font)
//    {
//        return;
//    }
//    const auto& path = font->face->face_name;
//    const auto& key = font->key;

//    html_fonts.erase(key);

//    auto it = fonts.find(path);
//    if(it != std::end(fonts))
//    {
//        auto use_count = it->second.use_count();
//        if(use_count == 1)
//        {
//            fonts.erase(it);
//        }
//    }
}

texture_ptr html_context::get_image(const std::string &src)
{
    auto it = images.find(src);
	if(it != images.end())
	{
		return it->second;
	}

    try
    {
        auto image = rend.create_texture(src);
        images[src] = image;
        return image;
    }
    catch (...)
    {
        return {};
    }
}

html_font_ptr html_context::get_font(size_t page_uid, const std::string& face_name, int size, int weight)
{

    if(face_name == "monospace")
    {
        return get_font(page_uid, options.default_monospace_font, size, weight);
    }

    std::string key = face_name + "#" + std::to_string(size) + "#"+ std::to_string(weight) + "#" + std::to_string(page_uid);
	auto it = html_fonts.find(key);
	if(it != html_fonts.end())
	{
		return it->second;
	}

	auto font = std::make_shared<html_font>();
    font->key = key;
	int bold_weight = 500;
	if(weight > bold_weight)
	{
		float step = (weight - bold_weight) / 100.0f;
		font->boldness = step * 0.1f;
	}

	constexpr int rasterize_size = 50;
	auto font_path = options.fonts_dir + "/" + face_name + ".ttf";

	auto find_it = fonts.find(font_path);
	if(find_it != std::end(fonts))
	{
		font->face = find_it->second;
	}
	else
	{
		glyphs_builder builder;
		builder.add(get_latin_glyph_range());
		builder.add(get_cyrillic_glyph_range());
		try
		{
			font->face = rend.create_font(create_font_from_ttf(font_path, builder.get(), rasterize_size, 2, true));
        }
		catch(...)
		{
            if(face_name != options.default_font)
            {
                return get_font(page_uid, options.default_font.c_str(), size, weight);
            }
            else
            {
                font->face = rend.create_font(create_default_font(rasterize_size, 2));
            }
		}
		fonts[font_path] = font->face;
	}

	font->scale = size / float(font->face->size);

	html_fonts[key] = font;

    return font;
}

}
