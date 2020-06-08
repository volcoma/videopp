#include "glyph_range.h"

#include <codecvt>
#include <locale>

namespace gfx
{
glyphs create_from_ranges(const fnt::font_wchar* ranges)
{
    glyphs result{};
    for(; ranges[0]; ranges += 2)
        result.emplace_back(ranges[0], ranges[1]);

    return result;
}

const glyphs& get_default_glyph_range()
{
    return get_latin_glyph_range();
}

const glyphs& get_latin_glyph_range()
{
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_latin());
    return range;
}

const glyphs& get_cyrillic_glyph_range()
{
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_cyrillic());
    return range;
}

const glyphs& get_arabic_glyph_range()
{
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_arabic());
    return range;
}

const glyphs& get_korean_glyph_range()
{
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_korean());
    return range;
}

const glyphs& get_chinese_glyph_range()
{
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_chinese_full());
    return range;
}

const glyphs& get_thai_glyph_range()
{
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_thai());
    return range;
}

const glyphs& get_chinese_simplified_common_glyph_range()
{    
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_chinese_simplified_common());
    return range;
}

const glyphs& get_chinese_simplified_official_glyph_range()
{
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_chinese_simplified_official());
    return range;
}

const glyphs& get_japanese_glyph_range()
{
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_japanese());
    return range;
}


const glyphs& get_currency_glyph_range()
{
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_currency());
    return range;
}

const glyphs& get_all_glyph_range()
{
    static const glyphs range = create_from_ranges(fnt::get_glyph_ranges_all());
    return range;
}

void glyphs_builder::add(const glyphs& g)
{
    std::copy(std::begin(g), std::end(g), std::back_inserter(range_));
}

const glyphs& glyphs_builder::get() const
{
    return range_;
}

namespace
{
std::wstring convert_from_unicode(const std::string& range)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring unicode_range = converter.from_bytes(range);

    size_t start_char = 0;
    while(true)
    {
        auto find = unicode_range.find(L"U+", start_char);
        if(find == std::string::npos)
        {
            break;
        }
        auto unicode = unicode_range.substr(find + 2, 4); // unicode is U+xxxx 4 characters
        unicode_range.erase(find, 6);
        start_char = find;

        auto unichar = static_cast<wchar_t>(stoi(unicode, nullptr, 16));
        unicode_range.insert(start_char, 1, unichar);
        start_char += 1;
    }

    return unicode_range;
}
}

glyphs parse_glyph_range(const std::string& range)
{
    auto unicode_range = convert_from_unicode(range);

    size_t start = 0, end = 0;
    glyphs result;
    while(true)
    {
        start = unicode_range.find_first_of(L'[', end);
        if(start == std::string::npos)
        {
            break;
        }
        start += 1;
        end = unicode_range.find_first_of(L']', start);

        if(end != std::string::npos && unicode_range.at(end - 1) == L'\\')
        {
            unicode_range.erase(end - 1, 1);
            end = unicode_range.find_first_of(L']', end);
        }

        if(end == std::string::npos)
        {
            break;
        }

        auto sub_str = unicode_range.substr(start, end - start);
        if(sub_str.size() == 3 && sub_str.at(1) == L'-') // there is range
        {
            auto start_ch = sub_str.front();
            auto end_ch = sub_str.back();
            if(start_ch > end_ch)
            {
                std::string utf8_str =
                    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(sub_str);
                throw gfx::exception(std::string("Range [") + utf8_str + "] not okay.");
            }

            result.emplace_back(start_ch, end_ch);
        }
        else
        {
            auto find = sub_str.find(L"\\-");
            if(find != std::string::npos)
            {
                sub_str.erase(find, 1);
            }

            for(auto& ch : sub_str)
            {
                result.emplace_back(ch, ch);
            }
        }
    }

    return result;
}
}
