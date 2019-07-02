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

struct rich_text_element
{
    using attr_table = std::unordered_map<std::string, std::string>;
    virtual ~rich_text_element() = default;
    virtual void draw(const math::transformf& trans, void* user_data) = 0;
    virtual rect get_rect() const = 0;
    virtual bool is_absolute() const { return false; }
};


struct rich_text_parser
{
    using creator_t = std::function<std::shared_ptr<rich_text_element>(const rich_text_element::attr_table&, const std::string&)>;
    using creator_container_t = std::unordered_map<std::string, creator_t>;
    using element_lines_t = std::vector<std::vector<std::shared_ptr<rich_text_element>>>;

    virtual ~rich_text_parser() = default;
    virtual element_lines_t parse(const std::string& text) const = 0;
    virtual void register_type(const std::string& name, creator_t creator);
protected:
    creator_container_t factory_;
};

using parser_ptr = std::shared_ptr<rich_text_parser>;

struct rich_text
{
    using element_lines_t = rich_text_parser::element_lines_t;

    void set_parser(const parser_ptr& parser);
    void set_utf8_text(const std::string& text);
    void set_line_gap(float gap);

    float get_line_gap() const;
    const element_lines_t& get_elements() const;

private:
    mutable std::shared_ptr<rich_text_element> background_element_;
    mutable element_lines_t elements_;

    std::string utf8_text_{};
    parser_ptr parser_{};
    float line_gap_{};
};


struct text_element : rich_text_element
{
    using font_getter_t = std::function<font_ptr(const std::string)>;

    text_element(const attr_table& table, const std::string& text, const font_getter_t& font_getter);
    void draw(const math::transformf& trans, void* user_data) override;
    rect get_rect() const override;
    text text_;
};

struct image_element : rich_text_element
{
    image_element(const attr_table& table, bool absolute);
    void draw(const math::transformf& trans, void* user_data) override;
    rect get_rect() const override;
    bool is_absolute() const override { return absolute_; }

    rect rect_{};
    color color_{};
    bool filled_{};
    bool absolute_{};
};


struct simple_html_parser : rich_text_parser
{
    element_lines_t parse(const std::string& text) const override;
private:
    void parse(element_lines_t& elements, const shared_ptr<html_element>& node, shared_ptr<html_element> parent) const;
};



}

