#include "html_context.h"
#include "../logger.h"
#include "../ttf_font.h"

#include <fstream>
#include <master.css>
namespace gfx
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

	builder.add(get_latin_glyph_range());
	builder.add(get_cyrillic_glyph_range());
	builder.add(get_currency_glyph_range());
}

void html_context::delete_font(html_font* font)
{
	if(!font)
	{
		return;
	}

    const auto& key = font->key;
    const auto& path = font->face->face_name;

	{
        auto it = html_fonts.find(key);
        if(it != std::end(html_fonts))
        {
            auto& html_font = it->second;
            html_font->ref--;
            if(html_font->ref == 0)
            {
                html_fonts.erase(it);
            }
        }
	}

    {
        auto it = fonts.find(path);
        if(it != std::end(fonts))
        {
            auto use_count = it->second.use_count();
            if(use_count == 1)
            {
                fonts.erase(it);
            }
        }
	}
}

texture_ptr html_context::get_image(const std::string& src)
{
	const auto& key = src;
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
	catch(...)
	{
		return {};
	}
}

html_font_ptr html_context::get_font(size_t page_uid, const std::string& face_name, int size, int weight,
									 bool italic, unsigned int decoration, bool is_fallback)
{

	bool sdf = options.default_font_options & font_flags::simulate_vectorization;
	bool simulate_bold = options.default_font_options & font_flags::simulate_bold;
	bool simulate_italic = options.default_font_options & font_flags::simulate_italic;
	bool use_kerning = options.default_font_options & font_flags::use_kerning;

	if(!sdf && simulate_bold)
	{
		sdf = true;
	}

	int bold_weight = 500;
	bool bold = weight > bold_weight;

	auto mapping_it = options.default_font_families.find(face_name);
	if(mapping_it != std::end(options.default_font_families))
	{
		const auto& family = mapping_it->second;
		if(bold && italic)
		{
			if(!simulate_bold && !simulate_italic)
			{
				return get_font(page_uid, family.bold_italic, size, weight, italic, decoration, is_fallback);
			}
		}

		if(bold && !simulate_bold)
		{
			return get_font(page_uid, family.bold, size, weight, italic, decoration, is_fallback);
		}

		if(italic && !simulate_italic)
		{
			return get_font(page_uid, family.italic, size, weight, italic, decoration, is_fallback);
		}

		return get_font(page_uid, family.regular, size, weight, italic, decoration, is_fallback);
	}

	std::string key = face_name;
	key += ":";
	key += std::to_string(size);
	key += ":";
	key += std::to_string(weight);
	key += ":";
	key += std::to_string(italic);
	key += ":";
	key += std::to_string(decoration);
	key += ":";
	key += std::to_string(page_uid);

	auto it = html_fonts.find(key);
	if(it != html_fonts.end())
	{
        it->second->ref++;
		return it->second;
	}

	auto font = std::make_shared<html_font>();
	font->ref++;
	font->key = key;
	if(bold && simulate_bold)
	{
		float step = (weight - bold_weight) / 100.0f;
		font->boldness = step * 0.1f;
	}

	if(italic && simulate_italic)
	{
		font->leaning = 3.0f;
	}

	font->underline = decoration & litehtml::font_decoration_underline;
    font->overline = decoration & litehtml::font_decoration_overline;
    font->linethrough = decoration & litehtml::font_decoration_linethrough;

	int rasterize_size = sdf ? 60 : size;
	int sdf_spread = sdf ? 2 : 0;
	const auto& fonts_key = sdf ? face_name : key;

	auto find_it = fonts.find(fonts_key);
	if(find_it != std::end(fonts))
	{
		font->face = find_it->second;
	}
	else
	{
		try
		{
            if(face_name == "embedded")
            {
                font->face = rend.create_font(create_default_font(float(rasterize_size), sdf_spread));
            }
            else
            {
                font->face = rend.create_font(
                    create_font_from_ttf(face_name, builder.get(), float(rasterize_size), sdf_spread, use_kerning));
            }
		}
		catch(const std::exception& e)
		{
			log(e.what());

            return {};
		}
		fonts[fonts_key] = font->face;
	}

	font->scale = float(size) / float(font->face->size);

	html_fonts[key] = font;

	return font;
}

} // namespace video_ctrl
