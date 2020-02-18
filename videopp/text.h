#pragma once

#include "font_ptr.h"
#include "vertex.h"
#include "rect.h"
#include "polyline.h"

#include <vector>
#include <regex>

namespace gfx
{
struct line_metrics
{
    /// Ascent of the line.
    float ascent{};
    /// Cap height of the line (H-height) is the height of a capital letter above the baseline.
    float cap_height{};
    /// X height of the line.
    float x_height{};
    /// Median of the line(cap_height/2).
    float median{};
    /// Baseline of the line.
    float baseline{};
    /// Descent of the line.
    float descent{};
    /// Min x of the line.
    float minx{};
    /// Max x of the line.
    float maxx{};
};


enum class script_line : uint32_t
{
    // ascender line.
	ascent,

    // cap height line.
    cap_height,

    // x_height line.
    x_height,

    // median line.
    median,

    // baseline.
	baseline,

    // descender line.
    descent,

    count
};

enum align : uint32_t
{
    // Horizontal align (general)
    left        = 1<<0,
    center      = 1<<1,
    right       = 1<<2,

    // Vertical align (general)
    top 		= 1<<3,
    middle      = 1<<4,
    bottom      = 1<<5,

    // Vertical align (text only)
    cap_height_top      = 1<<6, // cap_height of first line
    cap_height_bottom   = 1<<7, // cap_height of last line
    cap_height = cap_height_top,

    baseline_top        = 1<<8,  // baseline of first line
    baseline_bottom     = 1<<9,  // baseline of last line
    baseline = baseline_top,
};

using align_t = uint32_t;

float get_alignment_x(align_t alignment,
                      float minx,
                      float maxx,
                      bool pixel_snap);

float get_alignment_y(align_t alignment,
					  float miny,
					  float maxy,
					  bool pixel_snap);


std::pair<float, float> get_alignment_offsets(align_t alignment,
											  float minx, float miny,
											  float maxx, float maxy,
											  bool pixel_snap);



float get_alignment_y(align_t alignment,
					  float miny, float miny_baseline, float miny_cap,
					  float maxy, float maxy_baseline, float maxy_cap,
					  bool pixel_snap);

std::pair<float, float> get_alignment_offsets(align_t alignment,
											  float minx, float miny, float miny_baseline, float miny_cap,
											  float maxx, float maxy, float maxy_baseline, float maxy_cap,
											  bool pixel_snap);


struct text_decorator
{
    /// Begin glyph (inclusive).
	size_t begin_glyph{};

    /// End glyph (exclusive).
	size_t end_glyph{};

    /// Callback to be called once for the whole range
    std::function<float(bool add, float pen_x, float pen_y, size_t line)> callback;

    /// Extra advance
    math::vec2 advance{0, 0};

    uint32_t ignore_codepoint{};

    /// Scale to be used.
	float scale{1.0f};

    /// Leaning
    float leaning{};

    /// Color of the text
    color color_top = color::white();
    color color_bot = color::white();

    /// The line to align to.
    /// > median - superscript
    /// = median - normal (center based)
    /// < median - subscript
	script_line script{script_line::baseline};

    /// Kerning usage if the font provides any kerning pairs.
    bool kerning_enabled{};

};

class text
{
public:

	text() = default;
	text(const text&) = default;
	text& operator=(const text&) = default;
	text(text&&) = default;
	text& operator=(text&&) = default;
    ~text();
    //-----------------------------------------------------------------------------
    /// Set the utf8 text.
    //-----------------------------------------------------------------------------
    void set_utf8_text(const std::string& t);
	void set_utf8_text(std::string&& t);

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
    void set_alignment(uint32_t a);

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
    /// Sets the path of the line
    //-----------------------------------------------------------------------------
	void set_line_path(const polyline& line);
	void set_line_path(polyline&& line);

	void set_decorators(const std::vector<text_decorator>& decorators);
    void set_decorators(std::vector<text_decorator>&& decorators);
    void add_decorator(const text_decorator& decorators);
    void add_decorator(text_decorator&& decorators);


	//-----------------------------------------------------------------------------
    /// Gets the line_path of the text relative to the origin point
    //-----------------------------------------------------------------------------
	const polyline& get_line_path() const;

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
    color get_outline_color() const;

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
    color get_shadow_color_top() const;
    color get_shadow_color_bot() const;

    //-----------------------------------------------------------------------------
    /// Gets the font of the text.
    //-----------------------------------------------------------------------------
    const font_ptr& get_font() const;

    //-----------------------------------------------------------------------------
    /// Gets the alignment of the text (relative to the origin point).
    //-----------------------------------------------------------------------------
    align_t get_alignment() const;

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

    const std::vector<uint32_t>& get_unicode_text() const;
    //-----------------------------------------------------------------------------
    /// Gets the utf8 text.
    //-----------------------------------------------------------------------------
    const std::string& get_utf8_text() const;

    bool is_valid() const;

    void set_align_line_callback(const std::function<void(size_t, float)>& callback);
    void set_clear_geometry_callback(const std::function<void()>& callback);
    std::vector<text_decorator*> add_decorators(const std::regex& rx);

    static size_t count_glyphs(const std::string& utf8_text);
    static size_t count_glyphs(const char* utf8_text_begin, const char* utf8_text_end);

private:

    float get_advance_offset_x(const text_decorator& decorator) const;
    float get_advance_offset_y(const text_decorator& decorator) const;

    void clear_geometry();
    void clear_lines();
    void update_lines() const;
    void update_geometry(bool all) const;
    void update_unicode_text() const;
    const text_decorator* get_next_decorator(size_t glyph_idx, const text_decorator* current) const;

    bool get_decorator(size_t i, const text_decorator*& current, const text_decorator*& next) const;
    /// Buffer of quads.
    mutable std::vector<vertex_2d> geometry_;

    /// Lines of unicode codepoints.
    mutable std::vector<std::vector<uint32_t>> lines_;

    /// Lines metrics
    mutable std::vector<line_metrics> lines_metrics_;

    /// Unicode text
    mutable std::vector<uint32_t> unicode_text_;

    std::function<void(size_t, float)> align_line_callback;
    std::function<void()> clear_geometry_callback;

    /// Utf8 text
    std::string utf8_text_;

	/// Custom line path
	polyline line_path_;

    /// The font
    font_ptr font_;

    /// Rect of the text relative to the aligned origin.
    mutable frect rect_{};

    text_decorator decorator_{};

	std::vector<text_decorator> decorators_{};

    /// Shadow offsets of the text in pixels
    math::vec2 shadow_offsets_{0.0f, 0.0f};

    /// Total chars in the text.
    mutable uint32_t chars_ = 0;

    /// Outline color of the text
    color outline_color_ = color::black();

    /// Outline width of the text
    float outline_width_ = 0.0f;

    /// Shadow color of the text
    color shadow_color_top_ = color::black();
    color shadow_color_bot_ = color::black();

    /// Origin alignment
    align_t alignment_ = align::left | align::baseline_top;

    /// Max width
    float max_width_{};

};


}
