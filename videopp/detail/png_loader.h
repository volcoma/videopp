#pragma once

#include "../rect.h"
#include "../pixel_type.h"

#include <3rdparty/gli/gli/gli.hpp>

namespace gfx
{
    namespace detail
    {
        inline gli::format gli_pixtype_to_native_format(pix_type type)
        {
            switch(type)
            {
            case pix_type::gray:
                return gli::FORMAT_R8_UNORM_PACK8;
            case pix_type::rgb:
                return gli::FORMAT_RGB8_UNORM_PACK8;
            case pix_type::rgba:
                return gli::FORMAT_RGBA8_UNORM_PACK8;
            default:
                return gli::FORMAT_RGBA8_UNORM_PACK8;
            }
        }

        struct load_result
        {
            std::unique_ptr<gli::texture> texture {};
            pix_type type {};
            bool had_alpha_pixels_originally {};
        };

        load_result load_png(FILE* fp);
        bool save_png(FILE* fp, const std::unique_ptr<gli::texture>& texture, pix_type type,
                      size_t level, size_t layer, size_t face);
        std::tuple<rect, bool> is_png(FILE* fp);
    }
}
