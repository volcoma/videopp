#pragma once

#include "text.h"
#include "texture.h"
#include "draw_list.h"

namespace gfx
{

struct rich_text_builder
{
	void append(const std::string& text)
	{
		if(!result.empty())
		{
			result.append(" ");
		}
		result.append(text);
	}

	void append(const std::string& text, const std::string& tag)
	{
		if(!result.empty())
		{
			result.append(" ");
		}
		result.append(tag).append("(").append(text).append(")");
	}

	void append_formatted(const std::string& text)
	{
		if(!result.empty())
		{
			result.append(" ");
		}

        decorators.emplace_back();
        auto& decorator = decorators.back();
		decorator.scale = 0.4f;
		decorator.script = gfx::script_line::cap_height;
		decorator.unicode_range.begin = gfx::text::count_glyphs(result);
		decorator.unicode_range.end = decorator.unicode_range.begin + gfx::text::count_glyphs(text);

		result.append(text);
	}

	std::string result;
	std::vector<text_decorator> decorators;
};

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
	using text_getter_t = std::function<void(const std::string&, text& out)>;

	image_getter_t image_getter;
	text_getter_t text_getter;

    std::string image_tag{"image"};
    std::string video_tag{"video"};

	std::map<std::string, text_style> styles{};
	float line_height_scale = 1.0f;
};

class rich_text : public text
{
public:
	void calculate_wrap_fitting(math::transformf transform,
                                rect dst_rect,
                                size_fit sz_fit = size_fit::shrink_to_fit,
                                dimension_fit dim_fit = dimension_fit::uniform,
                                int tolerance = 0);

	void draw(draw_list& list, const math::transformf& transform) const;

	void set_config(const rich_config& cfg);
	void apply_config();

	bool set_utf8_text(const std::string& t);
	bool set_utf8_text(std::string&& t);
	void set_builder_results(rich_text_builder&& builder);

private:
	rect apply_constraints(const rect& r) const;
	void clear_embedded_elements();

	// begin-end pair
	using key_t = std::pair<size_t, size_t>;
	std::map<key_t, embedded_image> embedded_images_{};
	std::map<key_t, embedded_text> embedded_texts_{};

    mutable std::vector<embedded_image*> sorted_images_{};
    mutable std::vector<embedded_text*> sorted_texts_{};

	rich_config cfg_;

	math::transformf wrap_fitting_{};
    float calculated_line_height_{};
};


}
