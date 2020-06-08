#include "color.h"

namespace gfx
{
    namespace
    {
        // gray/black colors
        constexpr color black_color           {0x00, 0x00, 0x00};
        constexpr color state_gray_color      {0x70, 0x80, 0x90};
        constexpr color gray_color            {0x80, 0x80, 0x80};
        constexpr color silver_color          {0xC0, 0xC0, 0xC0};
        constexpr color gainsboro_color       {0xDC, 0xDC, 0xDC};

        // white colors
        constexpr color white_color           {0xFF, 0xFF, 0xFF};
        constexpr color snow_color            {0xFF, 0xFA, 0xFA};
        constexpr color ghost_while_color     {0xF8, 0xF8, 0xFF};
        constexpr color ivory_color           {0xFF, 0xFF, 0xF0};
        constexpr color linen_color           {0xFA, 0xF0, 0xE6};

        // purple/violet/magenta colors
        constexpr color lavender_color        {0xE6, 0xE6, 0xFA};
        constexpr color thistle_color         {0xD8, 0xBF, 0xD8};
        constexpr color violet_color          {0xEE, 0x82, 0xEE};
        constexpr color orchid_color          {0xDA, 0x70, 0xD6};
        constexpr color magenta_color         {0xFF, 0x00, 0xFF};
        constexpr color purple_color          {0x80, 0x00, 0x80};
        constexpr color indigo_color          {0x4B, 0x00, 0x82};

        // blue colors
        constexpr color navy_color            {0x00, 0x00, 0x80};
        constexpr color blue_color            {0x00, 0x00, 0xFF};
        constexpr color royal_blue_color      {0x41, 0x69, 0xE1};
        constexpr color sky_blue_color        {0x87, 0xCE, 0xEB};

        // cyan colors
        constexpr color teal_color            {0x00, 0x80, 0x80};
        constexpr color turquoise_color       {0x40, 0xE0, 0xD0};
        constexpr color aquamarine_color      {0x7F, 0xFF, 0xD4};
        constexpr color cyan_color            {0x00, 0xFF, 0xFF};

        //green colors
        constexpr color green_color           {0x00, 0x80, 0x00};
        constexpr color lime_color            {0x00, 0xFF, 0x00};
        constexpr color olive_color           {0x80, 0x80, 0x00};

        //brown colors
        constexpr color maroon_color          {0x80, 0x00, 0x00};
        constexpr color brown_color           {0xA5, 0x2A, 0x2A};
        constexpr color sienna_color          {0xA0, 0x52, 0x2D};
        constexpr color cholocate_color       {0xD2, 0x69, 0x1E};
        constexpr color peru_color            {0xCD, 0x85, 0x3F};
        constexpr color goldenrod_color       {0xDA, 0xA5, 0x20};
        constexpr color tan_color             {0xD2, 0xB4, 0x8C};
        constexpr color wheat_color           {0xF5, 0xDE, 0xB3};

        //yellow colors
        constexpr color gold_color            {0xFF, 0xD7, 0x00};
        constexpr color yellow_color          {0xFF, 0xFF, 0x00};

        //orange colors
        constexpr color orange_color          {0xFF, 0xA5, 0x00};
        constexpr color coral_color           {0xFF, 0x7F, 0x50};
        constexpr color tomato_color          {0xFF, 0x63, 0x47};

        //red colors
        constexpr color red_color             {0xFF, 0x00, 0x00};
        constexpr color crimson_color         {0xDC, 0x14, 0x3C};
        constexpr color salmon_color          {0xFA, 0x80, 0x72};

        //pink colors
        constexpr color pink_color            {0xFF, 0xC0, 0xCB};

        //transperat color
        constexpr color clear_color           {0x00, 0x00, 0x00, 0x00};
    }

    const color& color::black()
    {
        return black_color;
    }

    const color& color::state_gray()
    {
        return state_gray_color;
    }

    const color& color::gray()
    {
        return gray_color;
    }

    const color& color::silver()
    {
        return silver_color;
    }

    const color& color::gainsboro()
    {
        return gainsboro_color;
    }

    const color& color::white()
    {
        return white_color;
    }

    const color& color::snow()
    {
        return snow_color;
    }

    const color& color::ghost_while()
    {
        return ghost_while_color;
    }

    const color& color::ivory()
    {
        return ivory_color;
    }

    const color& color::linen()
    {
        return linen_color;
    }

    const color& color::lavender()
    {
        return lavender_color;
    }

    const color& color::thistle()
    {
        return thistle_color;
    }

    const color& color::violet()
    {
        return violet_color;
    }

    const color& color::orchid()
    {
        return orchid_color;
    }

    const color& color::magenta()
    {
        return magenta_color;
    }

    const color& color::purple()
    {
        return purple_color;
    }

    const color& color::indigo()
    {
        return indigo_color;
    }

    const color& color::navy()
    {
        return navy_color;
    }

    const color& color::blue()
    {
        return blue_color;
    }

    const color& color::royal_blue()
    {
        return royal_blue_color;
    }

    const color& color::sky_blue()
    {
        return sky_blue_color;
    }

    const color& color::teal()
    {
        return teal_color;
    }

    const color& color::turquoise()
    {
        return turquoise_color;
    }

    const color& color::aquamarine()
    {
        return aquamarine_color;
    }

    const color& color::cyan()
    {
        return cyan_color;
    }

    const color& color::green()
    {
        return green_color;
    }

    const color& color::lime()
    {
        return lime_color;
    }

    const color& color::olive()
    {
        return olive_color;
    }

    const color& color::maroon()
    {
        return  maroon_color;
    }

    const color& color::brown()
    {
        return brown_color;
    }

    const color& color::sienna()
    {
        return sienna_color;
    }

    const color& color::cholocate()
    {
        return cholocate_color;
    }

    const color& color::peru()
    {
        return peru_color;
    }

    const color& color::goldenrod()
    {
        return goldenrod_color;
    }

    const color& color::tan()
    {
        return tan_color;
    }

    const color& color::wheat()
    {
        return wheat_color;
    }

    const color& color::gold()
    {
        return gold_color;
    }

    const color& color::yellow()
    {
        return yellow_color;
    }

    const color& color::orange()
    {
        return orange_color;
    }

    const color& color::coral()
    {
        return coral_color;
    }

    const color& color::tomato()
    {
        return tomato_color;
    }

    const color& color::red()
    {
        return red_color;
    }

    const color& color::crimson()
    {
        return crimson_color;
    }

    const color& color::salmon()
    {
        return salmon_color;
    }

    const color& color::pink()
    {
        return pink_color;
    }

    const color& color::clear()
    {
        return clear_color;
    }

    bool color::operator==(const color &rhs) const noexcept
    {
        return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a;
    }

    bool color::operator!=(const color &rhs) const noexcept
    {
        return !(*this == rhs);
    }

    color &color::operator*=(const color &rhs) noexcept
    {
        r = static_cast<uint8_t> (((r / 255.0) * (rhs.r / 255.0)) * 255.0);
        g = static_cast<uint8_t> (((g / 255.0) * (rhs.g / 255.0)) * 255.0);
        b = static_cast<uint8_t> (((b / 255.0) * (rhs.b / 255.0)) * 255.0);
        a = static_cast<uint8_t> (((a / 255.0) * (rhs.a / 255.0)) * 255.0);

        return *this;
    }

}
