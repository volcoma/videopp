#include "rich_text.h"
#include "font.h"

#include <array>

namespace gfx
{

rich_text::rich_text(const rich_text& rhs)
    : text(rhs)
{
    cfg_ = rhs.cfg_;

    apply_config();
}

rich_text& rich_text::operator=(const rich_text& rhs)
{
    cfg_ = rhs.cfg_;

    static_cast<text&>(*this) = rhs;
    apply_config();
    return *this;
}

rich_text::rich_text(rich_text&& rhs) noexcept
    : text(std::move(rhs))
{
    cfg_ = rhs.cfg_;
    apply_config();
}

rich_text& rich_text::operator=(rich_text&& rhs) noexcept
{
    cfg_ = rhs.cfg_;

    static_cast<text&>(*this) = std::move(rhs);
    apply_config();
    return *this;
}

void rich_text::set_config(const rich_config& cfg)
{
    cfg_ = cfg;
    clear_lines();
    apply_config();
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
    apply_scripting_decorators();
    apply_config();
    return true;
}

bool rich_text::set_utf8_text(std::string&& t)
{
    if(!static_cast<text&>(*this).set_utf8_text(std::move(t)))
    {
        return false;
    }
    apply_scripting_decorators();
    apply_config();
    return true;
}

void rich_text::clear_lines()
{
    static_cast<text&>(*this).clear_lines();
    clear_embedded_elements();
}

void rich_text::apply_config()
{
    clear_embedded_elements();

    const auto& main_style = get_style();
    auto advance = main_style.advance;
    auto font = main_style.font;
    auto line_height = font ? font->line_height : 0.0f;

    calculated_line_height_ = (line_height * cfg_.image_scale + advance.y) * main_style.scale;

    // clear decorators with callbacks
    clear_decorators_with_callbacks();

    for(const auto& kvp : cfg_.styles)
    {
        const auto& style_id = kvp.first;
        const auto& style = kvp.second;
        auto decorators = add_decorators(style_id);

        for(const auto& decorator : decorators)
        {
            decorator->get_size_on_line = [this, style](const text_decorator& decorator, const line_metrics&, const char* str_begin, const char* str_end) -> text_decorator::size_info
            {
                key_t key{decorator.unicode_range.begin, decorator.unicode_range.end};

                static const line_metrics empty_metric {};

                auto it = embedded_texts_.find(key);
                if(it == std::end(embedded_texts_))
                {
                    auto& embedded = embedded_texts_[key];
                    sorted_texts_.emplace_back(&embedded);

                    embedded.text.set_style(style);
                    embedded.text.set_alignment(align::baseline_top | align::left);
                    embedded.text.set_overflow_type(overflow_type::none);

                    std::string tag{str_begin, str_end};
                    if(cfg_.text_getter)
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
                    const auto& line_metrics = embedded.text.get_lines_metrics();
                    const auto& first_line_metrics = line_metrics.empty() ? empty_metric : line_metrics.front();
                    return {element.rect.w, element.rect.h, {first_line_metrics}};
                }

                auto& embedded = it->second;
                auto& element = embedded.element;
                embedded.text.set_style(style);

                const auto& line_metrics = embedded.text.get_lines_metrics();
                const auto& first_line_metrics = line_metrics.empty() ? empty_metric : line_metrics.front();
                return {element.rect.w, element.rect.h, {first_line_metrics}};
            };

            decorator->set_position_on_line = [this](const text_decorator& decorator,
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
            decorator->get_size_on_line = [this](const text_decorator& decorator, const line_metrics& metrics, const char* str_begin, const char* str_end) -> text_decorator::size_info
            {
                const auto calc_image_line_metrics = [&](float image_alignment, float w, float h)
                {
                    const auto image_median = h * (1.0f - image_alignment);
                    const auto line_dist_median_to_baseline = metrics.baseline - metrics.median;

                    line_metrics res;
                    res.miny = res.ascent = image_median + line_dist_median_to_baseline;
                    res.median = res.cap_height = res.x_height = res.baseline = 0.0f;
                    res.descent = res.maxy = res.miny > h ? 0.0f : (res.miny - h);
                    res.minx = 0.0f;
                    res.maxx = w;
                    return res;
                };

                key_t key{decorator.unicode_range.begin, decorator.unicode_range.end};

                auto it = embedded_images_.find(key);
                if(it == std::end(embedded_images_))
                {
                    if(!cfg_.image_getter)
                    {
                        return {0.0f, 0.0f, {line_metrics()}};
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

                    const auto image_metrics = calc_image_line_metrics(cfg_.image_alignment, element.rect.w, element.rect.h);
                    return {element.rect.w, element.rect.h, {image_metrics}};
                }

                auto& embedded = it->second;
                auto& element = embedded.element;

                const auto image_metrics = calc_image_line_metrics(cfg_.image_alignment, element.rect.w, element.rect.h);
                return {element.rect.w, element.rect.h, {image_metrics}};
            };

            decorator->set_position_on_line = [this](const text_decorator& decorator,
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

                const auto& main_style = get_style();
                const auto& font = main_style.font;
                const auto scale = main_style.scale * get_small_caps_scale();

                element.line = line;
                element.rect.x = line_offset_x;
                element.rect.y = font ? (metrics.baseline - font->cap_height * scale * 0.5f) : metrics.median;
            };

        }
    }
}

void rich_text::apply_scripting_decorators()
{
    static const std::array<std::string, size_t(script_line::count)> scripts
    {{
            "_superscript_",
            "_superscript_cap_",
            "_unused1_",
            "_unused2_",
            "_subscript_base_",
            "_subscript_"
    }};

    for(size_t i = 0; i < scripts.size(); ++i)
    {
        const auto& script = scripts[i];
        auto decorators = add_decorators(script);

        for(auto& decorator : decorators)
        {
            decorator->scale = 0.58f;
            decorator->script = script_line(i);
        }
    }
}

frect rich_text::apply_line_constraints(const frect& r) const
{
    float aspect = r.w / r.h;
    auto result = r;
    result.h = float(int(calculated_line_height_));
    result.w = float(int(aspect * calculated_line_height_));
    return result;
}

float rich_text::get_calculated_line_height() const
{
    return calculated_line_height_;
}

const rich_config& rich_text::get_config() const
{
    return cfg_;
}

void rich_text::clear_embedded_elements()
{
    embedded_images_.clear();
    sorted_images_.clear();
    embedded_texts_.clear();
    sorted_texts_.clear();
}
}
