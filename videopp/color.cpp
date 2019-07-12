#include "color.h"

namespace video_ctrl
{
    const color color::black() { return {0, 0, 0, 255}; }
    const color color::blue() { return {0, 0, 255}; }
    const color color::cyan() { return {0, 255, 255, 255}; }
    const color color::gray() { return {128, 128, 128}; }
    const color color::green() { return {0, 255, 0}; }
    const color color::magenta() { return {255, 0, 255}; }
    const color color::red() { return {255, 0, 0}; }
    const color color::yellow() { return {255, 255, 0}; }
    const color color::white() { return {255, 255, 255}; }

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

    color &color::operator*=(float scalar) noexcept
    {
        r = static_cast<uint8_t> (((r / 255.0) * scalar) * 255.0);
        g = static_cast<uint8_t> (((g / 255.0) * scalar) * 255.0);
        b = static_cast<uint8_t> (((b / 255.0) * scalar) * 255.0);
        a = static_cast<uint8_t> (((a / 255.0) * scalar) * 255.0);

        return *this;
    }

    color operator *(const color& rhs, float scalar) noexcept
    {
        color c = rhs;
        c *= scalar;
        return c;
    }

}
