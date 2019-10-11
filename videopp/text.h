#pragma once

#include "font_ptr.h"
#include "vertex.h"
#include "rect.h"

#include <vector>

namespace video_ctrl
{
struct line_metrics
{
    /// Ascent of the line. Relative to the aligned origin.
    float ascent{};
    /// Baseline of the line. Relative to the aligned origin.
    float baseline{};
    /// Descent of the line. Relative to the aligned origin.
    float descent{};
    /// Min x of the line.. Relative to the aligned origin.
    float minx{};
    /// Max x of the line. Relative to the aligned origin.
    float maxx{};
    /// Min y of the line. Relative to the aligned origin.
    float miny{};
    /// Max y of the line. Relative to the aligned origin.
    float maxy{};
};

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

//    enum align : uint32_t
//    {
//        // Horizontal align
//        left        = 1<<0,
//        center      = 1<<1,
//        right       = 1<<2,
//        // Vertical align
//        top 		= 1<<3,
//        middle      = 1<<4,
//        bottom      = 1<<5,
//        baseline	= 1<<6,
//    };

    //-----------------------------------------------------------------------------
    /// Set the utf8 text.
    //-----------------------------------------------------------------------------
    void set_utf8_text(const std::string& t);

    //-----------------------------------------------------------------------------
    /// Set the font to be used.
    //-----------------------------------------------------------------------------
    void set_font(const font_ptr& f);

    //-----------------------------------------------------------------------------
    /// Sets the color of the text
    //-----------------------------------------------------------------------------
    void set_color(color c);
    void set_vgradient_colors(color top, color bot);

    //-----------------------------------------------------------------------------
    /// Set outline color of the text. Note this will only have any effect
    /// if the used font is vectorized(signed distance)
    //-----------------------------------------------------------------------------
    void set_outline_color(color c);

    //-----------------------------------------------------------------------------
    /// Set outline width of the text in range [0.0f, 0.4f]. Max outline width (0.4)
    /// will be the font's 'sdf_spread' thick in pixels.  Note this will only
    /// have any effect if the used font is vectorized(signed distance)
    //-----------------------------------------------------------------------------
    void set_outline_width(float owidth);

    //-----------------------------------------------------------------------------
    /// Set shadow color of the text. Note this will only have any effect
    /// if the shadow offsets are non zero
    //-----------------------------------------------------------------------------
    void set_shadow_color(color c);
    void set_shadow_vgradient_colors(color top, color bot);

    //-----------------------------------------------------------------------------
    /// Set shadow offsets of the text in pixels.
    //-----------------------------------------------------------------------------
    void set_shadow_offsets(const math::vec2& offsets);

    //-----------------------------------------------------------------------------
    /// Set extra advance between glyphs in pixels.
    //-----------------------------------------------------------------------------
    void set_advance(const math::vec2& advance);

    //-----------------------------------------------------------------------------
    /// Set point alignment based on position
    //-----------------------------------------------------------------------------
    void set_alignment(alignment a);

    //-----------------------------------------------------------------------------
    /// Enables/Disables kerning usage if the font provides any kerning pairs.
    //-----------------------------------------------------------------------------
    void set_kerning(bool enabled);

    //-----------------------------------------------------------------------------
    /// Sets glyph leaning to simulate Oblique style. It is in degrees
    /// increasing in clockwise direction. So a 12 degree leaning would mean
    /// to slant right.
    //-----------------------------------------------------------------------------
    void set_leaning(float leaning = 12.0f);

    //-----------------------------------------------------------------------------
    /// Sets the max with for wrapping
    //-----------------------------------------------------------------------------
    void set_max_width(float max_width);

    //-----------------------------------------------------------------------------
    /// Gets the width of the text. This takes into consideration any applied
    /// modifiers such as custom advance, outline width or shadow offsets
    //-----------------------------------------------------------------------------
    float get_width() const;

    //-----------------------------------------------------------------------------
    /// Gets the height of the text. This takes into consideration any applied
    /// modifiers such as custom advance, outline width or shadow offsets
    //-----------------------------------------------------------------------------
    float get_height() const;

    //-----------------------------------------------------------------------------
    /// Gets the height from the baseline of the topmost line of the text.
    /// This takes into consideration any applied modifiers such as custom advance,
    /// outline width or shadow offsets
    //-----------------------------------------------------------------------------
    float get_min_baseline_height() const;

    //-----------------------------------------------------------------------------
    /// Gets the height from the baseline of the bottommost line of the text.
    /// This takes into consideration any applied modifiers such as custom advance,
    /// outline width or shadow offsets
    //-----------------------------------------------------------------------------
    float get_max_baseline_height() const;

    //-----------------------------------------------------------------------------
    /// Gets the rect of the text relative to the aligned origin. Aligned to pixel
    //-----------------------------------------------------------------------------
    rect get_rect() const;

    //-----------------------------------------------------------------------------
    /// Gets the rect of the text relative to the aligned origin.
    //-----------------------------------------------------------------------------
    const frect& get_frect() const;

    //-----------------------------------------------------------------------------
    /// Gets the outline color of the text.
    //-----------------------------------------------------------------------------
    const color& get_outline_color() const;

    //-----------------------------------------------------------------------------
    /// Gets the outline width of the text.
    //-----------------------------------------------------------------------------
    float get_outline_width() const;

    //-----------------------------------------------------------------------------
    /// Gets the shadow offsets in pixels.
    //-----------------------------------------------------------------------------
    const math::vec2& get_shadow_offsets() const;

    //-----------------------------------------------------------------------------
    /// Gets the shadow color of the text.
    //-----------------------------------------------------------------------------
    const color& get_shadow_color_top() const;
    const color& get_shadow_color_bot() const;

    //-----------------------------------------------------------------------------
    /// Gets the font of the text.
    //-----------------------------------------------------------------------------
    const font_ptr& get_font() const;

    //-----------------------------------------------------------------------------
    /// Gets the alignment of the text (relative to the origin point).
    //-----------------------------------------------------------------------------
    alignment get_alignment() const;

    //-----------------------------------------------------------------------------
    /// Generates the geometry if needed
    /// buffer of quads - each 4 vertices form a quad
    //-----------------------------------------------------------------------------
    const std::vector<vertex_2d>& get_geometry() const;

    //-----------------------------------------------------------------------------
    /// Gets the lines metrics of the text.
    //-----------------------------------------------------------------------------
    const std::vector<line_metrics>& get_lines_metrics() const;

    //-----------------------------------------------------------------------------
    /// Gets the lines unicode codepoints of the texts.
    //-----------------------------------------------------------------------------
    const std::vector<std::vector<uint32_t>>& get_lines() const;

    //-----------------------------------------------------------------------------
    /// Gets the utf8 text.
    //-----------------------------------------------------------------------------
    const std::string& get_utf8_text() const;

    bool is_valid() const;

    static std::pair<float, float> get_alignment_offsets(text::alignment alignment,
                                   float minx, float miny, float maxx, float maxy, bool pixel_snap);

    bool debug = false;
private:
    float get_advance_offset_x() const;
    float get_advance_offset_y() const;

    void clear_geometry();
    void clear_lines();
    void update_lines() const;
    void update_geometry(bool all) const;
    void regen_unicode_text();

    /// Buffer of quads.
    mutable std::vector<vertex_2d> geometry_;

    /// Total chars in the text.
    mutable uint32_t chars_ = 0;

    /// Lines of unicode codepoints.
    mutable std::vector<std::vector<uint32_t>> lines_;

    /// Lines metrics
    mutable std::vector<line_metrics> lines_metrics_;

    /// Rect of the text relative to the aligned origin.
    mutable frect rect_{};

    /// Utf8 text
    std::string utf8_text_;

    /// Unicode text
    std::vector<uint32_t> unicode_text_;

    /// The font
    font_ptr font_;

    /// Color of the text
    color color_top_ = color::white();
    color color_bot_ = color::white();

    /// Outline color of the text
    color outline_color_ = color::black();

    /// Outline width of the text
    float outline_width_ = 0.0f;

    /// Shadow color of the text
    color shadow_color_top_ = color::black();
    color shadow_color_bot_ = color::black();

    /// Shadow offsets of the text in pixels
    math::vec2 shadow_offsets_{0.0f, 0.0f};

    /// Origin alignment
    alignment alignment_ = alignment::top_left;

    /// Extra advance
    math::vec2 advance_ = {0, 0};

    /// Leaning
    float leaning_{};

    /// Max width
    float max_width_{};

    /// Kerning usage if the font provides any kerning pairs.
    bool kerning_enabled_ = false;
};


}
