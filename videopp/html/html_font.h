#pragma once

#include "../font.h"

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

} // namespace video_ctrl
