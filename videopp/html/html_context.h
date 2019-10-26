#pragma once
#include "../color.h"
#include "../glyph_range.h"
#include "../rect.h"
#include "../renderer.h"
#include "../text.h"

#include "html_font.h"

#include <litehtml.h>

namespace gfx
{

struct html_defaults
{
	std::string default_font{};
	std::unordered_map<std::string, font_family> default_font_families;

	int default_font_size{};
	uint32_t default_font_options{};
};

struct html_context
{
	html_context(renderer& r, html_defaults opts);

	html_font_ptr get_font(size_t page_uid, const std::string& face_name, int size, int weight, bool italic,
						   unsigned int decoration, bool is_fallback = false);
	void delete_font(html_font* font);

	texture_ptr get_image(const std::string& src);
	bool load_file(const std::string& path, std::string& buffer);

	litehtml::context ctx;
	renderer& rend;
	html_defaults options;

	glyphs_builder builder;
	std::unordered_map<std::string, html_font_ptr> html_fonts;
	std::unordered_map<std::string, font_ptr> fonts;
	std::unordered_map<std::string, texture_ptr> images;
};

} // namespace video_ctrl
