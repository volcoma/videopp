#include "../png_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace gfx
{
    namespace detail
    {
        load_result load_png(FILE* fp)
        {
            load_result result {};

            int w = 0,h = 0,n = 0;
            uint8_t* data = stbi_load_from_file(fp, &w, &h, &n, 0);
            if(!data)
            {
                return {};
            }
            switch(n)
            {
            case 1:
                result.type = pix_type::gray;
                break;
            case 2:
                data = stbi__convert_format(data, n, 4, uint32_t(w), uint32_t(h));
                result.type = pix_type::rgba;
                result.had_alpha_pixels_originally = true;
                break;

            case 3:
                data = stbi__convert_format(data, n, 4, uint32_t(w), uint32_t(h));
                result.type = pix_type::rgba;
                break;
            case 4:
                result.type = pix_type::rgba;
                result.had_alpha_pixels_originally = true;
                break;
            default:
                stbi_image_free(data);
                return {};
            }

            result.texture = std::make_unique<gli::texture>(gli::target::TARGET_2D, detail::gli_pixtype_to_native_format(result.type),
                                                            gli::texture::extent_type{w, h, 1}, 1, 1, 1);
            ::memcpy(result.texture->data(0, 0, 0), data, size_t(w * h * bytes_per_pixel(result.type)));

            stbi_image_free(data);

            return result;
        }

        bool save_png(FILE* fp, const std::unique_ptr<gli::texture>& texture, pix_type type,
                      size_t level, size_t layer, size_t face)
        {
            int comp = 4;
            switch(type)
            {
            case pix_type::gray:
                comp = 1;
                break;
            case pix_type::rgb:
                comp = 3;
                break;
            case pix_type::rgba:
                comp = 4;
                break;
            }

            auto ext = texture->extent(level);
            int len {};
            auto data = reinterpret_cast<const uint8_t*>(texture->data(layer, face, level));
            auto png = stbi_write_png_to_mem(data, 0, ext.x, ext.y, comp, &len);
            if (!png)
            {
                return false;
            }

            fwrite(png, 1, size_t(len), fp);
            return true;
        }

        std::tuple<rect, bool> is_png(FILE* fp)
        {
            stbi__context s {};
            stbi__start_file(&s,fp);

            if (!stbi__png_test(&s))
            {
                return {};
            }

            if (!stbi__check_png_header(&s))
            {
                return {};
            }

            stbi__pngchunk c = stbi__get_chunk_header(&s);
            if (c.type != STBI__PNG_TYPE('I','H','D','R'))
            {
                return {};
            }
            gfx::rect rect {};

            rect.w = int(stbi__get32be(&s));
            rect.h = int(stbi__get32be(&s));

            return {rect, true};
        }
    }
}
