#include "rich_text.h"

namespace gfx
{
math::transformf rich_text::calculate_wrap_fitting(math::transformf transform,
												   gfx::rect dst_rect,
												   size_fit sz_fit,
												   dimension_fit dim_fit,
												   size_t depth)
{
	const auto& style = get_style();
	auto font = style.font.lock();
	auto line_height = font ? font->line_height : 0.0f;

	auto advance = (calculated_line_height_ - line_height);
	transform.translate(0.0f, advance * 0.25f, 0.0f);
	dst_rect.h -= int(advance * 0.25f);

	return align_wrap_and_fit_text(*this, transform, dst_rect, sz_fit, dim_fit, depth);
}

void rich_text::draw(draw_list& list, const math::transformf& transform) const
{
	list.add_text(*this, transform);

	for(const auto& kvp : embedded_images_)
	{
		const auto& embedded = kvp.second;
		const auto& element = embedded.element;
		auto image = embedded.data.image.lock();

		const auto& img_src_rect = embedded.data.src_rect;
		gfx::rect img_dst_rect = {0, 0, img_src_rect.w, img_src_rect.h};
		img_dst_rect = apply_constraints(img_dst_rect);

		img_dst_rect.x += int(element.x_offset);
		img_dst_rect.y += int(element.y_offset);
		img_dst_rect.y -= img_dst_rect.h / 2;

		list.add_image(image, img_src_rect, img_dst_rect, transform);
	}

	for(const auto& kvp : embedded_texts_)
	{
		const auto& embedded = kvp.second;
		const auto& element = embedded.element;
		const auto& text = embedded.text;

		math::transformf offset;
		offset.translate(element.x_offset, element.y_offset, 0);
		list.add_text(text, transform * offset);
	}
}

void rich_text::set_config(const rich_config& cfg)
{
	cfg_ = cfg;
}

void rich_text::apply_config()
{
	embedded_images_.clear();
	embedded_texts_.clear();

	const auto& style = get_style();
	auto font = style.font.lock();
	auto line_height = font ? font->line_height : 0.0f;
	calculated_line_height_ = line_height * cfg_.line_height_scale;
	auto advance = (calculated_line_height_ - line_height);

	set_advance({0, advance});

	set_align_line_callback([&](size_t line, float align_x)
	{
		for(auto& kvp : embedded_images_)
		{
			auto& element = kvp.second.element;
			if(element.line == line)
			{
				element.x_offset += align_x;
			}
		}

		for(auto& kvp : embedded_texts_)
		{
			auto& element = kvp.second.element;
			if(element.line == line)
			{
				element.x_offset += align_x;
			}
		}
	});

	set_clear_geometry_callback([&]()
	{
		embedded_images_.clear();
		embedded_texts_.clear();
	});

	set_decorators({});

	{
		auto decorators = add_decorators("img");

		for(const auto& decorator : decorators)
		{
			decorator->get_size_on_line = [&](const gfx::text_decorator& decorator, const char* str_begin, const char* str_end) -> float
			{
				key_t key{decorator.visual_range.begin, decorator.visual_range.end};

				auto it = embedded_images_.find(key);
				if(it == std::end(embedded_images_))
				{
					if(!cfg_.image_getter)
					{
						return 0.0f;
					}

					auto& embedded = embedded_images_[key];

					std::string utf8_str(str_begin, str_end);
					embedded.data = cfg_.image_getter(utf8_str);

					auto texture = embedded.data.image.lock();
					auto src_rect = embedded.data.src_rect;
					gfx::rect dst_rect = {0, 0, src_rect.w, src_rect.h};
					return float(apply_constraints(dst_rect).w);
				}

				auto& embedded = it->second;
				auto image = embedded.data.image;
				auto src_rect = embedded.data.src_rect;
				gfx::rect dst_rect = {0, 0, src_rect.w, src_rect.h};

				return float(apply_constraints(dst_rect).w);
			};

			decorator->set_position_on_line = [&](const gfx::text_decorator& decorator,
					float line_offset_x,
					size_t line,
					const gfx::line_metrics& metrics,
					const char* /*str_begin*/, const char* /*str_end*/)
			{
				key_t key{decorator.visual_range.begin, decorator.visual_range.end};

				auto& embedded = embedded_images_[key];
				auto& element = embedded.element;
				element.line = line;
				element.x_offset = line_offset_x;
				element.y_offset = metrics.median;
			};

		}
	}

	for(const auto& kvp : cfg_.styles)
	{
		const auto& style_id = kvp.first;
		const auto& style = kvp.second;
		auto decorators = add_decorators(style_id);

		for(const auto& decorator : decorators)
		{
			decorator->get_size_on_line = [&](const gfx::text_decorator& decorator, const char* str_begin, const char* str_end) ->float
			{
				key_t key{decorator.visual_range.begin, decorator.visual_range.end};

				auto it = embedded_texts_.find(key);
				if(it == std::end(embedded_texts_))
				{
					auto& embedded = embedded_texts_[key];
					embedded.text.set_style(style);
					embedded.text.set_alignment(gfx::align::baseline | gfx::align::left);
					embedded.text.set_utf8_text({str_begin, str_end});
					return embedded.text.get_width();
				}

				auto& embedded = it->second;
				auto& text = embedded.text;
				return text.get_width();
			};

			decorator->set_position_on_line = [&](const gfx::text_decorator& decorator,
					float line_offset_x,
					size_t line,
					const gfx::line_metrics& metrics,
					const char* /*str_begin*/, const char* /*str_end*/)
			{
				key_t key{decorator.visual_range.begin, decorator.visual_range.end};

				auto& embedded = embedded_texts_[key];
				auto& element = embedded.element;
				element.line = line;
				element.x_offset = line_offset_x;
				element.y_offset = metrics.baseline;
			};

		}
	}
}

rect rich_text::apply_constraints(rect r) const
{
	float aspect = float(r.w) / float(r.h);
	auto result = r;
	result.h = int(calculated_line_height_);
	result.w = int(aspect * calculated_line_height_);
	return result;
}

}
