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
	using image_getter_t = std::function<image_data(const std::string&)>;

	image_getter_t image_getter;
	std::map<std::string, gfx::text_style> styles{};

	float line_height_scale = 2.0f;
};

class rich_text : public gfx::text
{
public:
	math::transformf calculate_wrap_fitting(math::transformf transform, gfx::rect dst_rect, size_fit sz_fit = size_fit::shrink_to_fit,
											dimension_fit dim_fit = dimension_fit::uniform, size_t depth = 4);
	void draw(draw_list& list, const math::transformf& transform) const;

	void set_config(const rich_config& cfg);
	void apply_config();

private:
	gfx::rect apply_constraints(gfx::rect r) const;
	float calculated_line_height_{};

	// begin-end pair
	using key_t = std::pair<size_t, size_t>;
	std::map<key_t, embedded_image> embedded_images_{};
	std::map<key_t, embedded_text> embedded_texts_{};

	rich_config cfg_;
};


}
