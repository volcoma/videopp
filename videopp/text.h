#pragma once

#include "font_ptr.h"
#include "vertex.h"
#include "rect.h"
#include "polyline.h"

#include <vector>
#include <regex>

namespace gfx
{
enum align : uint32_t
{
    invalid                             = 0,
    // Horizontal align (general)
    left                                = 1<<0,
    center                              = 1<<1,
    right                               = 1<<2,
    horizontal_mask                     = left | center | right,

    // Vertical align (general)
    top                                 = 1<<3,
    middle                              = 1<<4,
    bottom                              = 1<<5,
    vertical_mask                       = top | middle | bottom,

    // Vertical align (text only)
    cap_height_top                      = 1<<6, // cap_height of first line
    cap_height_bottom                   = 1<<7, // cap_height of last line
    cap_height = cap_height_top,
    median                              = 1<<8, // half distance between cap_height and baseline
    baseline_top                        = 1<<9, // baseline of first line
    baseline_bottom                     = 1<<10,// baseline of last line
    baseline = baseline_bottom,
    typographic_mask                    = cap_height_top | cap_height_bottom | median | baseline_top | baseline_bottom,

//    geometric_top                 = 1<<11, // top of geometric bounds(visible characters)
//    geometric_middle              = 1<<12, // middle of geometric bounds(visible characters)
//    geometric_bottom              = 1<<13, // bottom of geometric bounds(visible characters)

//    geometric_mask                = geometric_top | geometric_middle | geometric_bottom,
    vertical_text_mask                  = vertical_mask | typographic_mask
};

using align_t = uint32_t;

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
    /// Min y of the line.
    float miny{};
    /// Max y of the line.
    float maxy{};
};

enum class script_line : uint32_t
{
    // Superscript aligned on the ascender line.
    ascent,

    // Superscript aligned on the cap height line.
    cap_height,

    // Superscript aligned on the x_height line.
    x_height,

    // Script aligned on the median line. (cap_height/2)
    median,

    // Subscript aligned on the baseline.
    baseline,

    // Subscript aligned on the descender line.
    descent,

    count
};


struct text_decorator
{
    struct size_info
    {
        float width;
        float height;
        line_metrics first_line_metrics;
    };

    using calc_size_t = std::function<size_info(
        const text_decorator& decorator,
        const line_metrics&,
        const char* str_begin,
        const char* str_end)>;

    using generate_line_t = std::function<void(
        const text_decorator& decorator,
        float line_offset_x,
        size_t line,
        const line_metrics& metrics,
        const char* str_begin,
        const char* str_end)>;

    struct range
    {
        bool contains(size_t idx) const;
        bool at_end(size_t idx) const;
        bool empty() const;
        /// Begin glyph (inclusive).
        size_t begin{};
        /// End glyph (exclusive).
        size_t end{};
    };

    bool is_visible(size_t idx) const;

    /// The range of unicode symbols to be matched
    range unicode_range{};

    /// The range of unicode symbols to be rendered (optional)
    range unicode_visual_range{};

    /// The utf8 range used for callbacks
    range utf8_visual_range{};

    /// External callback for size on line
    calc_size_t get_size_on_line;

    /// External callback for positioning something on the text line
    generate_line_t set_position_on_line;

    /// Scale to be used.
    float scale{1.0f};

    /// The line to align to.
    /// > median - superscript
    /// = median - normal (center based)
    /// < median - subscript
    script_line script{script_line::baseline};
};

struct text_style
{
    /// The font
    font_ptr font{};

    /// Extra advance
    math::vec2 advance{0, 0};

    /// Color of the text
    color color_top{color::white()};
    color color_bot{color::white()};

    /// Outline color of the text
    color outline_color_top{color::black()};
    color outline_color_bot{color::black()};

    /// Shadow offsets of the text in pixels
    math::vec2 shadow_offsets{0.0f, 0.0f};

    /// Shadow color of the text
    color shadow_color_top{color::black()};
    color shadow_color_bot{color::black()};
    float shadow_softness{0.0};

    /// Softness of the text
    float softness{0.0};

    /// Outline width of the text
    float outline_width{0.0f};

    /// Scale
    float scale{1.0f};

    /// Leaning
    float leaning{0.0f};

    /// Outline affets char spacing
    bool outline_advance{true};

    /// Kerning usage if the font provides any kerning pairs.
    bool kerning_enabled{false};
};
using text_style_ptr = std::shared_ptr<text_style>;

enum class overflow_type
{
    /// Break words when overflowing if no break characters
    /// like 'spaces' are presnet (useful for Chinese, Japanese)
    /// as they don't have any standalone 'spaces'.
    word,

    /// Keep words' integrity when overflowing.
    word_break,

    /// Do not break lines.
    none,
};


class text
{
public:

    enum class line_height_behaviour
    {
        fixed,   // All lines have the height of the tallest one.
        dynamic, // Each line has the height of it's tallest decorator.
    };

    text() = default;
    text(const text&) = default;
    text& operator=(const text&) = default;
    text(text&&) = default;
    text& operator=(text&&) = default;
    ~text();
    //-----------------------------------------------------------------------------
    /// Set the utf8 text.
    //-----------------------------------------------------------------------------
    bool set_utf8_text(const std::string& t);
    bool set_utf8_text(std::string&& t);


    void clear_lines();

    //-----------------------------------------------------------------------------
    /// Set the whole style at once.
    //-----------------------------------------------------------------------------
    void set_style(const text_style& style);

    //-----------------------------------------------------------------------------
    /// Set the font to be used. An override font size
    /// can be used. Scaling will be applied if it differs
    /// from the font one.
    //-----------------------------------------------------------------------------
    void set_font(const font_ptr& f, int sz_override = -1);

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
    void set_outline_vgradient_colors(color top, color bot);
    //-----------------------------------------------------------------------------
    /// Set outline width of the text in range [0.0f, 0.4f]. Max outline width (0.4)
    /// will be the font's 'sdf_spread' thick in pixels.  Note this will only
    /// have any effect if the used font is vectorized(signed distance)
    //-----------------------------------------------------------------------------
    void set_outline_width(float owidth);

    //-----------------------------------------------------------------------------
    /// Outline affets char spacing
    //-----------------------------------------------------------------------------
    void set_outline_advance(bool oadvance);

    //-----------------------------------------------------------------------------
    /// Set softness of the text in range [0.0f, 1.0f]. Note this will only
    /// have any effect if the used font is vectorized(signed distance)
    //-----------------------------------------------------------------------------
    void set_softness(float softness);

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
    void set_alignment(align_t flag);

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
    void set_wrap_width(float max_width);

    //-----------------------------------------------------------------------------
    /// Sets the overflow type
    //-----------------------------------------------------------------------------
    void set_overflow_type(overflow_type overflow);

    //-----------------------------------------------------------------------------
    /// Sets the line height behaviour.
    //-----------------------------------------------------------------------------
    void set_line_height_behaviour(line_height_behaviour behaviour);

    //-----------------------------------------------------------------------------
    /// Sets the path of the line
    //-----------------------------------------------------------------------------
    void set_line_path(const polyline& line);
    void set_line_path(polyline&& line);

    //-----------------------------------------------------------------------------
    /// Set shadow softness of the text in range [0.0f, 1.0f]. Note this will only
    /// have any effect if the used font is vectorized(signed distance)
    //-----------------------------------------------------------------------------
    void set_shadow_softness(float softness);

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

    float get_wrap_width() const;
    //-----------------------------------------------------------------------------
    /// Gets the rect of the text relative to the aligned origin. Aligned to pixel
    //-----------------------------------------------------------------------------
    rect get_rect() const;

    //-----------------------------------------------------------------------------
    /// Gets the rect of the text relative to the aligned origin.
    //-----------------------------------------------------------------------------
    enum class bounds_query
    {
        typographic, // ascent to descent based
        precise      // based on alignment
                     // - ascent line to descent line
                     // - cap line to base line
                     // - geometric bounds(top, bottom) of current text
    };

    frect get_bounds(bounds_query query = bounds_query::typographic) const;

    //-----------------------------------------------------------------------------
    /// Gets the style of the text
    //-----------------------------------------------------------------------------
    const text_style& get_style() const;

    //-----------------------------------------------------------------------------
    /// Gets the alignment of the text (relative to the origin point).
    //-----------------------------------------------------------------------------
    align_t get_alignment() const noexcept;

    //-----------------------------------------------------------------------------
    /// Sets an opacity of the text. It multiplies by the style colors' alpha.
    //-----------------------------------------------------------------------------
    void set_opacity(float opacity);

    //-----------------------------------------------------------------------------
    /// Gets the opacity of the text
    //-----------------------------------------------------------------------------
    float get_opacity() const noexcept;

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
    /// Gets the unicode text.
    //-----------------------------------------------------------------------------
    const std::vector<uint32_t>& get_unicode_text() const;

    //-----------------------------------------------------------------------------
    /// Gets the utf8 text.
    //-----------------------------------------------------------------------------
    const std::string& get_utf8_text() const;

    //-----------------------------------------------------------------------------
    /// Gets the overflow type of the text.
    //-----------------------------------------------------------------------------
    overflow_type get_overflow_type() const;

    //-----------------------------------------------------------------------------
    /// Gets the line height behaviour
    //-----------------------------------------------------------------------------
    auto get_line_height_behaviour() const -> line_height_behaviour;

    //-----------------------------------------------------------------------------
    /// Checks the validity of the text.
    //-----------------------------------------------------------------------------
    bool is_valid() const;

    //-----------------------------------------------------------------------------
    /// Adds/sets text decorators
    //-----------------------------------------------------------------------------
    void set_decorators(const std::vector<text_decorator>& decorators);
    void set_decorators(std::vector<text_decorator>&& decorators);
    void add_decorator(const text_decorator& decorators);
    void add_decorator(text_decorator&& decorators);

    //-----------------------------------------------------------------------------
    /// Adds text decorators
    //-----------------------------------------------------------------------------
    std::vector<text_decorator*> add_decorators(const std::string& style_id);
    std::vector<text_decorator*> add_decorators(const std::regex& global_matcher, const std::regex& local_visual_matcher={});
    std::vector<text_decorator*> add_decorators(const std::string& start_str,
                                                const std::string& end_str);

    //-----------------------------------------------------------------------------
    /// Returns the main decorator
    //-----------------------------------------------------------------------------
    const text_decorator& get_decorator() const;
    std::vector<text_decorator>& acess_decorators() { return decorators_; }

    static size_t count_glyphs(const std::string& utf8_text);
    static size_t count_glyphs(const char* utf8_text_begin, const char* utf8_text_end);

    void clear_decorators_with_callbacks();

    void set_small_caps(bool small_caps);
    bool get_small_caps() const;

    float get_small_caps_scale() const;
    float get_line_height() const;
private:
    float get_advance_offset_x() const;
    float get_advance_offset_y() const;

    void clear_geometry();
    void update_lines() const;
    void update_geometry() const;
    void update_unicode_text() const;
    void update_alignment() const;

    color apply_opacity(color c) const noexcept;
    void set_scale(float scale);

    const text_decorator* get_next_decorator(size_t glyph_idx, const text_decorator* current) const;
    bool get_decorator(size_t i, const text_decorator*& current, const text_decorator*& next) const;
    float get_decorator_scale(const text_decorator* current) const;

    /// Buffer of quads.
    mutable std::vector<vertex_2d> geometry_;

    /// Lines of unicode codepoints.
    mutable std::vector<std::vector<uint32_t>> lines_;

    /// Lines metrics
    mutable std::vector<line_metrics> lines_metrics_;

    /// Unicode text
    mutable std::vector<uint32_t> unicode_text_;

    /// Utf8 text
    std::string utf8_text_{};

    /// Custom line path
    polyline line_path_{};

    /// Rect of the text relative to the aligned origin.
    mutable frect rect_{};

    /// main decorator
    text_decorator main_decorator_{};

    /// user decorators
    std::vector<text_decorator> decorators_{};

    /// Style of the text
    text_style style_{};

    /// Total chars in the text.
    mutable uint32_t chars_{0};

    /// Origin alignment
    align_t alignment_ = align::top | align::left;

    /// Overflow type
    overflow_type overflow_ = overflow_type::word;

    /// Max width
    mutable float max_wrap_width_{};

    /// Opacity
    float opacity_{1.0f};

    /// Line height behaviour
    line_height_behaviour line_height_behaviour_ = line_height_behaviour::fixed;

    bool small_caps_{};
};


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


std::string to_string(text::line_height_behaviour behaviour);
std::string to_string(overflow_type overflow);

template<typename T>
T from_string(const std::string&);

template<>
text::line_height_behaviour from_string<text::line_height_behaviour>(const std::string& str);

template<>
overflow_type from_string<overflow_type>(const std::string& str);


}
