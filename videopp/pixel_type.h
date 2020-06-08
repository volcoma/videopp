#pragma once

namespace gfx
{
    enum class pix_type
    {
        gray = 1,
        rgb = 3,
        rgba = 4
    };

    inline int bytes_per_pixel(pix_type type)
    {
        return static_cast<int> (type);
    }
}
