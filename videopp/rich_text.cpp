#include "rich_text.h"
#include "font.h"

namespace gfx
{
namespace
{
bool begins_with(const std::string& str, const std::string& value)
{
	// Validate requirements
	if(str.length() < value.length())
	{
		return false;
	}
	if(str.empty() || value.empty())
	{
		return false;
	}

	// Do the subsets match?
	return str.substr(0, value.length()) == value;
}

bool ends_with(const std::string& s, const std::string& suffix)
{
	if (suffix.size() > s.size())
	{
		return false;
	}
	return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}
}


std::vector<embedded_image*> rich_text::get_embedded_images() const
{
	return sorted_images_;
}

std::vector<embedded_text*> rich_text::get_embedded_texts() const
{
	return sorted_texts_;
}

bool rich_text::set_utf8_text(const std::string& t)
{
	if(!static_cast<text&>(*this).set_utf8_text(t))
	{
		return false;
	}
	apply_config();
	return true;
}

bool rich_text::set_utf8_text(std::string&& t)
{
	if(!static_cast<text&>(*this).set_utf8_text(std::move(t)))
	{
		return false;
	}
	apply_config();
	return true;
}

void rich_text::clear_lines()
{
	static_cast<text&>(*this).clear_lines();
	clear_embedded_elements();
}

void rich_text::set_config(const rich_config& cfg)
{
	cfg_ = cfg;
}

void rich_text::apply_config()
{
	clear_embedded_elements();

	const auto& style = get_style();
	auto font = style.font;
	auto line_height = font ? font->line_height : 0.0f;
	calculated_line_height_ = line_height * cfg_.line_height_scale;
	auto advance = (calculated_line_height_ - line_height);

	set_advance({0, advance});

	set_decorators({});

	for(const auto& kvp : cfg_.styles)
	{
		const auto& style_id = kvp.first;
		const auto& style = kvp.second;
		auto decorators = add_decorators(style_id);

		for(const auto& decorator : decorators)
		{
			decorator->get_size_on_line = [&](const text_decorator& decorator, const char* str_begin, const char* str_end) -> std::pair<float, float>
			{
				key_t key{decorator.unicode_range.begin, decorator.unicode_range.end};

				auto it = embedded_texts_.find(key);
				if(it == std::end(embedded_texts_))
				{
					auto& embedded = embedded_texts_[key];
					sorted_texts_.emplace_back(&embedded);

					embedded.text.set_style(style);
					embedded.text.set_alignment(align::baseline | align::left);

					std::string tag{str_begin, str_end};
					bool dynamic = begins_with(tag, "__") && ends_with(tag, "__");
					if(dynamic && cfg_.text_getter)
					{
						cfg_.text_getter(tag, embedded.text);

						if(embedded.text.get_utf8_text().empty())
						{
							embedded.text.set_utf8_text(std::move(tag));
						}
					}
					else
					{
						embedded.text.set_utf8_text(std::move(tag));
					}

					auto& element = embedded.element;
					element.rect = {0, 0, embedded.text.get_width(), embedded.text.get_height()};
					return {element.rect.w, element.rect.h};
				}

				auto& embedded = it->second;
				auto& element = embedded.element;

				return {element.rect.w, element.rect.h};
			};

			decorator->set_position_on_line = [&](const text_decorator& decorator,
												  float line_offset_x,
												  size_t line,
												  const line_metrics& metrics,
												  const char* /*str_begin*/, const char* /*str_end*/)
			{
				key_t key{decorator.unicode_range.begin, decorator.unicode_range.end};

				auto& embedded = embedded_texts_[key];
				auto& element = embedded.element;
				element.line = line;
				element.rect.x = line_offset_x;
				element.rect.y = metrics.baseline;
			};

		}
	}

	{
		auto decorators = add_decorators(cfg_.image_tag);

		for(const auto& decorator : decorators)
		{
			decorator->get_size_on_line = [&](const text_decorator& decorator, const char* str_begin, const char* str_end) -> std::pair<float, float>
			{
				key_t key{decorator.unicode_range.begin, decorator.unicode_range.end};

				auto it = embedded_images_.find(key);
				if(it == std::end(embedded_images_))
				{
					if(!cfg_.image_getter)
					{
						return {0.0f, 0.0f};
					}

					auto& embedded = embedded_images_[key];
					sorted_images_.emplace_back(&embedded);

					std::string utf8_str(str_begin, str_end);
					cfg_.image_getter(utf8_str, embedded.data);

					if(!embedded.data.src_rect)
					{
						embedded.data.src_rect = {0, 0, int(calculated_line_height_), int(calculated_line_height_)};
					}

					auto& element = embedded.element;
					auto texture = embedded.data.image.lock();
					auto& src_rect = embedded.data.src_rect;
					element.rect = {0.0f, 0.0f, float(src_rect.w), float(src_rect.h)};
					element.rect = apply_line_constraints(element.rect);

					return {element.rect.w, element.rect.h};
				}

				auto& embedded = it->second;
				auto& element = embedded.element;
				return {float(element.rect.w), float(element.rect.h)};
			};

			decorator->set_position_on_line = [&](const text_decorator& decorator,
												  float line_offset_x,
												  size_t line,
												  const line_metrics& metrics,
												  const char* /*str_begin*/, const char* /*str_end*/)
			{
				key_t key{decorator.unicode_range.begin, decorator.unicode_range.end};

				auto it = embedded_images_.find(key);
				if(it == std::end(embedded_images_))
				{
					return;
				}

				auto& embedded = it->second;
				auto& element = embedded.element;

				element.line = line;
				element.rect.x = line_offset_x;
				element.rect.y = metrics.median;
				element.rect.y -= float(element.rect.h) * 0.5f;
			};

		}
	}
}

frect rich_text::apply_line_constraints(const frect& r) const
{
	float aspect = r.w / r.h;
	auto result = r;
	result.h = calculated_line_height_;
	result.w = aspect * calculated_line_height_;
	return result;
}

float rich_text::get_calculated_line_height() const
{
	return calculated_line_height_;
}

void rich_text::clear_embedded_elements()
{
	embedded_images_.clear();
	sorted_images_.clear();
	embedded_texts_.clear();
	sorted_texts_.clear();
}

}
