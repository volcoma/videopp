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
            }
        }
        else
        {
            text_.set_font(default_font());
        }
    }

}

void text_decorator::draw(math::transformf &trans, draw_list &list)
{
    list.add_text(text_, trans);
    trans.translate(text_.get_width() * trans.get_scale().x, 0.0f, 0.0f);
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

void rect_decorator::draw(math::transformf &trans, draw_list &list)
{
    list.add_rect(rect_, trans, color_);
    trans.translate(rect_.w * trans.get_scale().x, 0.0f, 0.0f);
}

void rich_text::add_decorator(const std::string &name, rich_text::creator_t creator)
{
    factory_[name] = std::move(creator);
}

void rich_text::set_utf8_text(const std::string &text)
{
    if(utf8_text_ == text)
    {
        return;
    }
    utf8_text_ = text;
    decorators_.clear();
}

void rich_text::parse() const
{
    if(!decorators_.empty())
    {
        return;
    }
    html_parser parser;
    auto doc = parser.parse(utf8_text_);
    parse(doc->root());
}

void rich_text::parse(const shared_ptr<html_element> &node) const
{
    const auto& name = node->get_name();
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

        decorators_.emplace_back();
        auto& decorator = decorators_.back();
        decorator = creator(table, text);
    }

    const auto& children = node->get_children();
    for(const auto& child : children)
    {
        parse(child);
    }
}

const std::vector<std::shared_ptr<decorator> > rich_text::get_decorators() const
{
    parse();
    return decorators_;
}

}
