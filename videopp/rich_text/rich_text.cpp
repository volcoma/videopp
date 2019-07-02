#include "rich_text.h"
#include "csscolorparser.hpp"
#include "../draw_list.h"

#include <sstream>


namespace video_ctrl
{

text_element::text_element(const rich_text_element::attr_table &table, const std::string &text, const font_getter_t& font_getter)
{
    text_.set_utf8_text(text);

    std::stringstream ss;

    {
        auto it = table.find("color");
        if(it != std::end(table))
        {
            color col = html_color_parser::parse(it->second);
            text_.set_color(col);
        }
        else
        {
            text_.set_color(color::black());
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

void text_element::draw(const math::transformf& trans, void* user_data)
{
    auto& list = *reinterpret_cast<draw_list*>(user_data);
    list.add_text(text_, trans);
}

rect text_element::get_rect() const
{
    return text_.get_rect();
}

image_element::image_element(const rich_text_element::attr_table &table, bool absolute)
{
    absolute_ = absolute;
    std::stringstream ss;
    {
        auto it = table.find("background-color");
        if(it != std::end(table))
        {
            color_ = html_color_parser::parse(it->second);
        }
    }
    {
        auto it = table.find("color");
        if(it != std::end(table))
        {
            color_ = html_color_parser::parse(it->second);
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
        auto it = table.find("width");
        if(it != std::end(table))
        {
            rect_.w = std::stoi(it->second);
        }
    }

    {
        auto it = table.find("height");
        if(it != std::end(table))
        {
            rect_.h = std::stoi(it->second);
        }
    }
}

void image_element::draw(const math::transformf& trans, void* user_data)
{
    if(rect_)
    {
        auto& list = *reinterpret_cast<draw_list*>(user_data);
        list.add_rect(rect_, trans, color_);
    }
}

rect image_element::get_rect() const
{
    return rect_;
}


void rich_text_parser::register_type(const std::string &name, creator_t creator)
{
    factory_[name] = std::move(creator);
}


rich_text_parser::element_lines_t simple_html_parser::parse(const std::string& text) const
{

    html_parser parser;
    element_lines_t elements;
    auto doc = parser.parse(text);

    parse(elements, doc->root(), nullptr);
    return elements;
}

void simple_html_parser::parse(element_lines_t& elements, const shared_ptr<html_element>& node, shared_ptr<html_element> parent) const
{
    auto text = node->text();
    const auto& name = node->get_name();
    const auto& children = node->get_children();

    if(name == "br" || name == "p")
    {
        elements.emplace_back();
    }

    auto find_it = factory_.find(name);
    if(find_it != std::end(factory_))
    {
        const auto& creator = find_it->second;
        rich_text_element::attr_table table;
        const auto& attributes = node->get_attributes();

        for(const auto& kvp : attributes)
        {
            table[kvp.first] = kvp.second;
        }

        while(parent)
        {
            const auto& parent_attributes = parent->get_attributes();
            for(const auto& kvp : parent_attributes)
            {
                table.insert(kvp);
            }

            parent = parent->get_parent();
        }


        if(elements.empty())
        {
            elements.emplace_back();
        }
        elements.back().emplace_back();
        auto& rich_text_element = elements.back().back();
        rich_text_element = creator(table, text);
    }



    for(const auto& child : children)
    {
        parse(elements, child, node);
    }
}

void rich_text::set_parser(const parser_ptr& parser)
{
    parser_ = parser;
    elements_.clear();
}

void rich_text::set_utf8_text(const std::string& text)
{
    if(utf8_text_ == text)
    {
        return;
    }
    utf8_text_ = text;
    elements_.clear();
}

void rich_text::set_line_gap(float gap)
{
    line_gap_ = gap;
}

float rich_text::get_line_gap() const
{
    return line_gap_;
}

const rich_text::element_lines_t& rich_text::get_elements() const
{
    if(elements_.empty())
    {
        elements_ = parser_->parse(utf8_text_);
    }
    return elements_;
}

}
