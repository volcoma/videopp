#pragma once
#include "../color.h"
#include "../glyph_range.h"
#include "../rect.h"
#include "../renderer.h"
#include "../text.h"
#include "../ttf_font.h"

#include <litehtml.h>

namespace video_ctrl
{

struct font_family
{
	std::string regular;
	std::string italic;
	std::string bold;
	std::string bold_italic;
};

namespace font_flags
{
enum : uint32_t
{
	none = 0,
	simulate_vectorization = 2 << 0,
	simulate_bold = 2 << 1,
	simulate_italic = 2 << 2,
	use_kerning = 2 << 3,

	simulate_all = simulate_vectorization | simulate_bold | simulate_italic,
};
}

struct html_defaults
{
	std::string default_font{};
	std::unordered_map<std::string, font_family> default_font_families;

	int default_font_size{};
	uint32_t default_font_options{};
};

struct html_font
{
	size_t ref{};
	font_ptr face;
	std::string key;

	// data to be used during drawing
	// instead of different fonts
	float scale{1.0f};
	float boldness{0.0f};
	float leaning{0.0f};
	bool underline{};
	bool overline{};
	bool linethrough{};

};
using html_font_ptr = std::shared_ptr<html_font>;

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
