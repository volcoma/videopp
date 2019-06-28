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
    virtual void draw(math::transformf& trans, draw_list& list) = 0;
};

struct rich_text
{
    using creator_t = std::function<std::shared_ptr<decorator>(const decorator::attr_table&, const std::string&)>;
    using creator_container_t = std::unordered_map<std::string, creator_t>;
    using decorator_container_t = std::vector<std::shared_ptr<decorator>>;

    void add_decorator(const std::string& name, creator_t creator);
    void set_utf8_text(const std::string& text);
    const decorator_container_t get_decorators() const;

private:
    void parse() const;
    void parse(const shared_ptr<html_element>& node) const;

    creator_container_t factory_;
    mutable decorator_container_t decorators_;
    std::string utf8_text_;
};


struct text_decorator : decorator
{
    using font_getter_t = std::function<font_ptr(const std::string)>;

    text_decorator(const attr_table& table, const std::string& text, const font_getter_t& font_getter);
     ~text_decorator() override = default;
    void draw(math::transformf& trans, draw_list& list) override;

    text text_;
};

struct rect_decorator : decorator
{
    rect_decorator(const attr_table& table);
     ~rect_decorator() override = default;
    void draw(math::transformf& trans, draw_list& list) override;

    rect rect_{};
    color color_{};
    bool filled_{};
};



}

