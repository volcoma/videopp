#include "rich_text.h"
#include "../draw_list.h"

#include <sstream>


namespace video_ctrl
{

text_decorator::text_decorator(const decorator::attr_table &table, const std::string &text, const font_getter_t& font_getter)
{
    text_.set_utf8_text(text);

    std::stringstream ss;

    {
        auto it = table.find("color");
        if(it != std::end(table))
        {
            ss << std::hex << it->second;

            uint32_t hex_col{};
            ss >> hex_col;
            color col(hex_col);

            text_.set_color(col);
        }
    }
    {
        auto it = table.find("font");
        if(it != std::end(table))
        {
            if(font_getter)
            {
                auto font = font_getter(it->second);

                if(font)
                {
                    text_.set_font(font);
                }
                else
                {
                    text_.set_font(default_font());
                }
            }
        }
        else
        {
            text_.set_font(default_font());
        }
    }

}

void text_decorator::draw(draw_list& list, const math::transformf& trans)
{
    list.add_text(text_, trans);
}

rect text_decorator::get_rect() const
{
    return text_.get_rect();
}

rect_decorator::rect_decorator(const decorator::attr_table &table)
{
    std::stringstream ss;

    {
        auto it = table.find("color");
        if(it != std::end(table))
        {
            ss << std::hex << it->second;

            uint32_t hex_col{};
            ss >> hex_col;
            color_ = color(hex_col);
        }
    }

    {
        auto it = table.find("x");
        if(it != std::end(table))
        {
            rect_.x = std::stoi(it->second);
        }
    }

    {
        auto it = table.find("y");
        if(it != std::end(table))
        {
            rect_.y = std::stoi(it->second);
        }
    }

    {
        auto it = table.find("w");
        if(it != std::end(table))
        {
            rect_.w = std::stoi(it->second);
        }
    }

    {
        auto it = table.find("h");
        if(it != std::end(table))
        {
            rect_.h = std::stoi(it->second);
        }
    }
}

void rect_decorator::draw(draw_list& list, const math::transformf& trans)
{
    list.add_rect(rect_, trans, color_);
}

rect rect_decorator::get_rect() const
{
    return rect_;
}


void rich_text_parser::register_type(const std::string &name, creator_t creator)
{
    factory_[name] = std::move(creator);
}


rich_text_parser::decorator_lines_t simple_html_parser::parse(const std::string& text) const
{

    html_parser parser;
    decorator_lines_t decorators;
    auto doc = parser.parse(text);

    parse(decorators, doc->root());
    return decorators;
}

void simple_html_parser::parse(decorator_lines_t& decorators, const shared_ptr<html_element>& node) const
{
    const auto& name = node->get_name();

    if(name == "br")
    {
        decorators.emplace_back();
    }

    auto text = node->text();

    auto find_it = factory_.find(name);
    if(find_it != std::end(factory_))
    {
        const auto& creator = find_it->second;
        decorator::attr_table table;
        const auto& attributes = node->get_attributes();
        for(const auto& kvp : attributes)
        {
            table[kvp.first] = kvp.second;
        }

        if(decorators.empty())
        {
            decorators.emplace_back();
        }
        decorators.back().emplace_back();
        auto& decorator = decorators.back().back();
        decorator = creator(table, text);
    }

    const auto& children = node->get_children();
    for(const auto& child : children)
    {
        parse(decorators, child);
    }
}

void rich_text::set_parser(const parser_ptr& parser)
{
    parser_ = parser;
    decorators_.clear();
}

void rich_text::set_utf8_text(const std::string& text)
{
    if(utf8_text_ == text)
    {
        return;
    }
    utf8_text_ = text;
    decorators_.clear();
}

void rich_text::set_line_gap(float gap)
{
    line_gap_ = gap;
}

float rich_text::get_line_gap() const
{
    return line_gap_;
}

const rich_text::decorator_lines_t& rich_text::get_decorators() const
{
    if(decorators_.empty())
    {
        decorators_ = parser_->parse(utf8_text_);
    }
    return decorators_;
}

}
