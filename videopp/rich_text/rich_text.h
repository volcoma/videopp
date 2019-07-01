#pragma once

#include "html_parser.hpp"
#include "../color.h"
#include "../rect.h"
#include "../text.h"


#include <cstdint>
#include <functional>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

namespace video_ctrl
{
struct draw_list;

struct decorator
{
    using attr_table = std::unordered_map<std::string, std::string>;
    virtual ~decorator() = default;
    virtual void draw(draw_list& list, const math::transformf& trans) = 0;
    virtual rect get_rect() const = 0;
};


struct rich_text_parser
{
    using creator_t = std::function<std::shared_ptr<decorator>(const decorator::attr_table&, const std::string&)>;
    using creator_container_t = std::unordered_map<std::string, creator_t>;
    using decorator_lines_t = std::vector<std::vector<std::shared_ptr<decorator>>>;

    virtual ~rich_text_parser() = default;
    virtual decorator_lines_t parse(const std::string& text) const = 0;
    virtual void register_type(const std::string& name, creator_t creator);
protected:
    creator_container_t factory_;
};

using parser_ptr = std::shared_ptr<rich_text_parser>;

struct rich_text
{
    using decorator_lines_t = rich_text_parser::decorator_lines_t;

    void set_parser(const parser_ptr& parser);
    void set_utf8_text(const std::string& text);
    void set_line_gap(float gap);

    float get_line_gap() const;
    const decorator_lines_t& get_decorators() const;

private:
    mutable decorator_lines_t decorators_;

    float line_gap_{};
    parser_ptr parser_;
    std::string utf8_text_;
};


struct text_decorator : decorator
{
    using font_getter_t = std::function<font_ptr(const std::string)>;

    text_decorator(const attr_table& table, const std::string& text, const font_getter_t& font_getter);
    void draw(draw_list& list, const math::transformf& trans) override;
    rect get_rect() const override;
    text text_;
};

struct rect_decorator : decorator
{
    rect_decorator(const attr_table& table);
    void draw(draw_list& list, const math::transformf& trans) override;
    rect get_rect() const override;

    rect rect_{};
    color color_{};
    bool filled_{};
};


struct simple_html_parser : rich_text_parser
{
    decorator_lines_t parse(const std::string& text) const override;
private:
    void parse(decorator_lines_t& decorators, const shared_ptr<html_element>& node) const;
};



}

