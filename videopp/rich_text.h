#pragma once

#include "text.h"
#include "texture.h"

namespace gfx
{


struct line_element
{
	size_t line{};
    frect rect{};
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

	std::string image_tag{"image"};
	std::string video_tag{"video"};

	image_getter_t image_getter;
	text_getter_t text_getter;

	std::map<std::string, text_style> styles{};
	float line_height_scale = 1.0f;
};

class rich_text : public text
{
public:

	rich_text() = default;
	rich_text(const rich_text&) ;
	rich_text& operator=(const rich_text&);
	rich_text(rich_text&&) noexcept;
	rich_text& operator=(rich_text&&) noexcept;

	void set_config(const rich_config& cfg);
	bool set_utf8_text(const std::string& t);
	bool set_utf8_text(std::string&& t);
    void clear_lines();

	std::vector<embedded_image*> get_embedded_images() const;
	std::vector<embedded_text*> get_embedded_texts() const;
	float get_calculated_line_height() const;
private:
    frect apply_line_constraints(const frect& r) const;
	void apply_config();
	void clear_embedded_elements();

	// begin-end pair
	using key_t = std::pair<size_t, size_t>;
	std::map<key_t, embedded_image> embedded_images_{};
	std::map<key_t, embedded_text> embedded_texts_{};

    mutable std::vector<embedded_image*> sorted_images_{};
    mutable std::vector<embedded_text*> sorted_texts_{};

	rich_config cfg_;

    float calculated_line_height_{};
};


struct text_builder
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


	std::string result;
	std::vector<text_decorator> decorators;
};

template<typename T>
inline void apply_builder(const text_builder& builder, T& text)
{
	if(text.set_utf8_text(builder.result))
	{
		for(auto& decorator : builder.decorators)
		{
			text.add_decorator(decorator);
		}
	}
}


}
