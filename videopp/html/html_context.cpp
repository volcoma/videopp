#include "html_context.h"
#include <master.css>
#include <fstream>

namespace video_ctrl
{

namespace detail
{
template <typename Container = std::string, typename CharT = char, typename Traits = std::char_traits<char>>
auto read_stream(std::basic_istream<CharT, Traits>& in, Container& container) -> bool
{
	static_assert(
		// Allow only strings...
		std::is_same<Container,
					 std::basic_string<CharT, Traits, typename Container::allocator_type>>::value ||
			// ... and vectors of the plain, signed, and
			// unsigned flavours of CharT.
			std::is_same<Container, std::vector<CharT, typename Container::allocator_type>>::value ||
			std::is_same<Container, std::vector<std::make_unsigned_t<CharT>,
												typename Container::allocator_type>>::value ||
			std::is_same<Container,
						 std::vector<std::make_signed_t<CharT>, typename Container::allocator_type>>::value,
		"only strings and vectors of ((un)signed) CharT allowed");

	auto const start_pos = in.tellg();
	if(std::streamsize(-1) == start_pos)
	{
		return false;
	};

	if(!in.seekg(0, std::ios_base::end))
	{
		return false;
	};

	auto const end_pos = in.tellg();

	if(std::streamsize(-1) == end_pos)
	{
		return false;
	};

	auto const char_count = end_pos - start_pos;

	if(!in.seekg(start_pos))
	{
		return false;
	};

	container.resize(static_cast<std::size_t>(char_count));

	if(!container.empty())
	{
		if(!in.read(reinterpret_cast<CharT*>(&container[0]), char_count))
		{
			return false;
		};
	}

	return true;
}
} // namespace detail

bool html_context::load_file(const std::string& path, std::string& buffer)
{
	std::ifstream stream(path, std::ios::in | std::ios::binary);

	if(!stream.is_open())
	{
		return false;
	}

	return detail::read_stream(stream, buffer);
}

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
    std::string key = options.images_dir + "/" + src;
    auto it = images.find(key);
	if(it != images.end())
	{
		return it->second;
	}

    try
    {
        auto image = rend.create_texture(key);
        if(image)
        {
            images[key] = image;
        }
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
                return get_font(page_uid, options.default_font, size, weight);
            }

            font->face = rend.create_font(create_default_font(rasterize_size, 2));
		}
		fonts[font_path] = font->face;
	}

	font->scale = size / float(font->face->size);

	html_fonts[key] = font;

    return font;
}

}
