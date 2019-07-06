#pragma once

#include "font_ptr.h"
#include "math/glm_includes.h"
#include "vertex.h"
#include "rect.h"

#include <vector>

namespace video_ctrl
{

class text
{
public:
    enum class alignment : uint32_t
    {
        top,
        bottom,
        left,
        right,
        center,
        top_left,
        top_right,
        bottom_left,
        bottom_right,

        baseline_top,
        baseline_top_left,
        baseline_top_right,
        baseline_bottom,
        baseline_bottom_left,
        baseline_bottom_right,
        count
    };

    text();
    ~text();

    void set_utf8_text(const std::string& t);
    void set_font(const font_ptr& f);

    void set_color(color c);

    void set_outline_color(color c);
    void set_outline_width(float owidth);

    void set_shadow_color(color c);
    void set_shadow_offsets(const math::vec2& offsets);

    void set_advance(const math::vec2& advance);

    // point alignment based on position
    void set_alignment(alignment a);

    // maximum width of text in units
    // 0 = no maximum
    void set_max_width(float w);

    float get_width() const;
    float get_height() const;
    float get_min_baseline_height() const;
    float get_max_baseline_height() const;

    video_ctrl::rect get_rect() const;

    const color& get_color() const;

    const color& get_outline_color() const;
    float get_outline_width() const;

    const math::vec2& get_shadow_offsets() const;
    const color& get_shadow_color() const;

    const font_ptr& get_font() const;

    // generates the geometry if needed
    // buffer of quads - each 4 vertices form a quad
    const std::vector<vertex_2d>& get_geometry() const;
    const std::vector<std::vector<uint32_t>>& get_lines() const;
    const std::vector<float>& get_ascent_lines() const;
    const std::vector<float>& get_descent_lines() const;
    const std::vector<float>& get_baseline_lines() const;

    const std::string& get_utf8_text() const;

    bool is_valid() const;
    float get_advance_offset_x() const;
    float get_advance_offset_y() const;

    void set_kerning(bool enabled);
private:
    void clear_geometry();
    void clear_lines();

    void update_lines() const;
    void update_geometry() const;
    void regen_unicode_text();
    // buffer of quads
    mutable std::vector<vertex_2d> geometry_;
    // lines
    mutable uint32_t chars_ = 0;
    mutable std::vector<std::vector<uint32_t>> lines_;

    mutable std::vector<float> ascent_lines_;
    mutable std::vector<float> descent_lines_;
    mutable std::vector<float> baseline_lines_;

    mutable frect rect_{};
    mutable float min_baseline_height_{};
    mutable float max_baseline_height_{};

    std::string utf8_text_;
    std::vector<uint32_t> unicode_text_;

    float lean_{};
    font_ptr font_;

    color color_ = color::white();

    color outline_color_ = color::black();
    float outline_width_ = 0.0f;

    color shadow_color_ = color::black();
    math::vec2 shadow_offsets_{0.0f, 0.0f};

    alignment alignment_ = alignment::top_left;
    float max_width_ = 0;
    math::vec2 advance_ = {0, 0};
    bool kerning_enabled_ = false;
};


rect align_rect(const rect& r, text::alignment align);
rect align_rect(const frect& r, text::alignment align);

}
