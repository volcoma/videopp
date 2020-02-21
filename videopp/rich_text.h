#pragma once

#include "text.h"
#include "texture.h"
#include "draw_list.h"

namespace gfx
{
struct line_element
{
	size_t line{};
	float x_offset{};
	float y_offset{};
};

struct image_data
{
	gfx::rect src_rect{};
	gfx::texture_weak_ptr image{};
};

struct embedded_image
{
	line_element element{};
	image_data data{};
};

struct embedded_text
{
	line_element element{};
	gfx::text text{};
};


struct rich_config
{
	using image_getter_t = std::function<void(const std::string&, image_data& out)>;
	using dynamic_text_getter_t = std::function<void(const std::string&, text& out)>;

	image_getter_t image_getter;
	dynamic_text_getter_t dynamic_text_getter;

	std::map<std::string, text_style> styles{};
	float line_height_scale = 2.0f;
};

struct rich_text_builder
{
	void append(const std::string& text)
	{
		result.append(text);
	}

	void append(const std::string& text, const std::string& tag)
	{
		result.append(tag).append("(").append(text).append(")");
	}

	std::string result;
};


class rich_text : public text
{
public:
	void calculate_wrap_fitting(math::transformf transform, rect dst_rect, size_fit sz_fit = size_fit::shrink_to_fit,
						   dimension_fit dim_fit = dimension_fit::uniform, size_t depth = 40);



	void draw(draw_list& list, const math::transformf& transform) const;

	void set_config(const rich_config& cfg);
	void apply_config();

	void set_utf8_text(const std::string& t);
	void set_utf8_text(std::string&& t);
private:
	rect apply_constraints(rect r) const;
	float calculated_line_height_{};

	// begin-end pair
	using key_t = std::pair<size_t, size_t>;
	std::map<key_t, embedded_image> embedded_images_{};
	std::map<key_t, embedded_text> embedded_texts_{};

	rich_config cfg_;

	math::transformf wrap_fitting_{};
};


}
