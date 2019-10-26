#pragma once

#include "font.h"

#include <set>

namespace gfx
{
    using glyph_range = std::pair<char_t, char_t>;
    using glyphs = std::vector<glyph_range>;

    const glyphs& get_default_glyph_range();
    const glyphs& get_latin_glyph_range();
    const glyphs& get_cyrillic_glyph_range();
    const glyphs& get_korean_glyph_range();

    // Full chinese is around 25000 symbols
    const glyphs& get_chinese_glyph_range();
    const glyphs& get_chinese_simplified_glyph_range();
    const glyphs& get_japanese_glyph_range();
    const glyphs& get_thai_glyph_range();
    const glyphs& get_currency_glyph_range();


    struct glyphs_builder
    {
        void add(const glyphs& range);
        const glyphs& get() const;

    private:
        glyphs range_;
    };

    // parse from string with this format:
    // range start with [ and end with ]
    // [...][...]...[...]
    // Inner range format:
    // 1. [<start_char>-<end_char>]
    // 2. [<char_1><char_2>...<char_n>]
    // char formating: for special chars like []- use \\ in front of this chars
    // for utf8 encode Can you U+XXXX (XXXX is id of char in utf8 table)
    glyphs parse_glyph_range(const std::string &range);
}
