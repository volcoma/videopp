#include "rich_text.h"

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


void rich_text::calculate_wrap_fitting(math::transformf transform,
										rect dst_rect,
										size_fit sz_fit,
										dimension_fit dim_fit,
										int tolerance)
{
	const auto& style = get_style();
	auto font = style.font;
	auto line_height = font ? font->line_height : 0.0f;

	auto advance = (calculated_line_height_ - line_height);
	transform.translate(0.0f, advance * 0.5f, 0.0f);
	dst_rect.h -= int(advance);

	wrap_fitting_ = align_wrap_and_fit_text(*this, transform, dst_rect, sz_fit, dim_fit, tolerance);
}

void rich_text::set_utf8_text(const std::string& t)
{
	static_cast<text&>(*this).set_utf8_text(t);
	apply_config();
}

void rich_text::set_utf8_text(std::string&& t)
{
	static_cast<text&>(*this).set_utf8_text(std::move(t));
    apply_config();
}

void rich_text::set_builder_results(rich_text_builder&& builder)
{
    set_utf8_text(std::move(builder.result));
    for(auto& decorator : builder.decorators)
    {
        add_decorator(std::move(decorator));
    }
}

void rich_text::draw(draw_list& list, const math::transformf& transform) const
{
	auto world = transform * wrap_fitting_;

	list.add_text(*this, world);

    std::sort(std::begin(sorted_texts_), std::end(sorted_texts_), [](const auto& lhs, const auto& rhs)
    {
        return lhs->text.get_style().font < rhs->text.get_style().font;
    });

	for(const auto& ptr : sorted_texts_)
	{
		const auto& embedded = *ptr;
		const auto& element = embedded.element;
		const auto& text = embedded.text;

		math::transformf offset;
		offset.translate(element.x_offset, element.y_offset, 0);
		list.add_text(text, world * offset);
	}

    std::sort(std::begin(sorted_images_), std::end(sorted_images_), [](const auto& lhs, const auto& rhs)
    {
        return lhs->data.image.lock() < rhs->data.image.lock();
    });

	for(const auto& ptr : sorted_images_)
	{
		const auto& embedded = *ptr;
		const auto& element = embedded.element;
		auto image = embedded.data.image.lock();

		const auto& img_src_rect = embedded.data.src_rect;
		rect img_dst_rect = {0, 0, img_src_rect.w, img_src_rect.h};
		img_dst_rect = apply_constraints(img_dst_rect);

		img_dst_rect.x += int(element.x_offset);
		img_dst_rect.y += int(element.y_offset);
		img_dst_rect.y -= img_dst_rect.h / 2;

		list.add_image(image, img_src_rect, img_dst_rect, world);
	}
}

void rich_text::set_config(const rich_config& cfg)
{
	cfg_ = cfg;
}

void rich_text::apply_config()
{
	clear_embedded_elements();
	wrap_fitting_ = {};

	const auto& style = get_style();
	auto font = style.font;
	auto line_height = font ? font->line_height : 0.0f;
	calculated_line_height_ = line_height * cfg_.line_height_scale;
	auto advance = (calculated_line_height_ - line_height);

	set_advance({0, advance});

	set_clear_lines_callback([&]()
	{
		clear_embedded_elements();
	});

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


                    return {embedded.text.get_width(), embedded.text.get_height()};
				}

				auto& embedded = it->second;
                return {embedded.text.get_width(), embedded.text.get_height()};
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
				element.x_offset = line_offset_x;
				element.y_offset = metrics.baseline;
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

					auto texture = embedded.data.image.lock();
					auto src_rect = embedded.data.src_rect;

					rect dst_rect = {0, 0, src_rect.w, src_rect.h};
                    dst_rect = apply_constraints(dst_rect);
                    return {float(dst_rect.w), float(dst_rect.h)};
				}

				auto& embedded = it->second;
				auto image = embedded.data.image;
				auto src_rect = embedded.data.src_rect;
				rect dst_rect = {0, 0, src_rect.w, src_rect.h};

                dst_rect = apply_constraints(dst_rect);
                return {float(dst_rect.w), float(dst_rect.h)};
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
				element.x_offset = line_offset_x;
				element.y_offset = metrics.median;
			};

		}
	}
}

rect rich_text::apply_constraints(const rect& r) const
{
	float aspect = float(r.w) / float(r.h);
	auto result = r;
	result.h = int(calculated_line_height_);
	result.w = int(aspect * calculated_line_height_);
    return result;
}

void rich_text::clear_embedded_elements()
{
    embedded_images_.clear();
    sorted_images_.clear();
    embedded_texts_.clear();
    sorted_texts_.clear();
}

}
