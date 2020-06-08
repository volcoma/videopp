#include "../png_loader.h"

#include "../../utils.h"

#include <libpng16/png.h>

namespace gfx
{
    namespace detail
    {
        namespace png
        {
            struct file_reader
            {
                png_structp png_ptr = nullptr;
                png_infop info_ptr = nullptr;

                ~file_reader()
                {
                    if (png_ptr != nullptr)
                    {
                        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
                    }
                }
            };

            const auto warrning_fn = [](png_structp, png_const_charp) {

            };

            const auto read_fn = [](png_structp ptr, png_bytep buffer, png_size_t size)
            {
                auto fp = reinterpret_cast<FILE*>(png_get_io_ptr(ptr));
                auto result = fread(buffer, size, 1, fp);
                if (result != 1)
                {
                    throw gfx::exception("Cannot read input file.");
                }
            };
        }

        load_result load_png(FILE* fp)
        {
            load_result result {};

            { //check file is it png.
                uint8_t png_header[8] = {0x00, };
                auto result = fread(png_header, sizeof(png_header), 1, fp);
                if (result <= 0)
                {
                    return {};
                }

                if (png_sig_cmp(png_header, 0, sizeof(png_header)))
                {
                    return {};
                }
            }

            png::file_reader file_reader;

            file_reader.png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, png::warrning_fn);

            if (!file_reader.png_ptr)
            {
                return {};
            }

            png_set_read_fn(file_reader.png_ptr, fp, png::read_fn);

            //png_init_io(file_reader.png_ptr, fp.get());
            png_set_sig_bytes(file_reader.png_ptr, 8);

            file_reader.info_ptr = png_create_info_struct(file_reader.png_ptr);
            if (!file_reader.info_ptr)
            {
                return {};
            }
            png_read_info(file_reader.png_ptr, file_reader.info_ptr);

            size png_size {};
            png_size.w = static_cast<int> (png_get_image_width(file_reader.png_ptr, file_reader.info_ptr));
            png_size.h = static_cast<int> (png_get_image_height(file_reader.png_ptr, file_reader.info_ptr));
            auto color_type = png_get_color_type(file_reader.png_ptr, file_reader.info_ptr);
            auto bit_depth = png_get_bit_depth(file_reader.png_ptr, file_reader.info_ptr);

            if (bit_depth == 16 /* bits per color */)
            {
                // set strip for 16 bits to 8 bits. We need only 8bits per color quality.
                png_set_strip_16(file_reader.png_ptr);
            }

            switch (color_type)
            {
            case PNG_COLOR_TYPE_GRAY:
                if (bit_depth < 8 /* bits per color */)
                {
                    png_set_expand_gray_1_2_4_to_8(file_reader.png_ptr);
                }
                result.type = pix_type::gray;
                result.had_alpha_pixels_originally = true;
                break;

            case PNG_COLOR_TYPE_GRAY_ALPHA:
                // this type is always 8 or 16 bits but we already strip 16 bits
                png_set_gray_to_rgb(file_reader.png_ptr);
                if (png_get_valid(file_reader.png_ptr, file_reader.info_ptr, PNG_INFO_tRNS))
                {
                    png_set_tRNS_to_alpha(file_reader.png_ptr);
                }
                result.type = pix_type::rgba;
                result.had_alpha_pixels_originally = true;
                break;

            case PNG_COLOR_TYPE_RGB:
                // set alpha for opengl texture create optimization
                //log("WARNING: Input file " + file_name + " is RGB format. Please convert it to RGBA format.");
                png_set_filler(file_reader.png_ptr, 0xFF, PNG_FILLER_AFTER);
                result.type = pix_type::rgba;
                result.had_alpha_pixels_originally = false;
                break;

            case PNG_COLOR_TYPE_RGBA:
                result.type = pix_type::rgba;
                result.had_alpha_pixels_originally = true;
                break;

            case PNG_COLOR_TYPE_PALETTE:
                //log("WARNING: Input file " + file_name +
                //    " is COLOR PALETTE format. Please convert it to RGBA format.");
                png_set_palette_to_rgb(file_reader.png_ptr);
                if (png_get_valid(file_reader.png_ptr, file_reader.info_ptr, PNG_INFO_tRNS))
                {
                    png_set_tRNS_to_alpha(file_reader.png_ptr);
                    result.had_alpha_pixels_originally = true;
                }
                else
                {
                    png_set_filler(file_reader.png_ptr, 0xFF, PNG_FILLER_AFTER);
                    result.had_alpha_pixels_originally = false;
                }
                result.type = pix_type::rgba;
                break;

            default:
                return {};
            }

            png_read_update_info(file_reader.png_ptr, file_reader.info_ptr);
            auto pitch = png_get_rowbytes(file_reader.png_ptr, file_reader.info_ptr);

            result.texture = std::make_unique<gli::texture>(gli::target::TARGET_2D, detail::gli_pixtype_to_native_format(result.type),
                                                            gli::texture::extent_type{png_size.w, png_size.h, 1}, 1, 1, 1);
            std::vector<uint8_t*> row_pointers;
            row_pointers.reserve(static_cast<uint32_t> (png_size.h));

            auto data = reinterpret_cast<uint8_t*> (result.texture->data());
            for (uint32_t i = 0; i < static_cast<uint32_t> (png_size.h); i++)
            {
                row_pointers.push_back(data + i * pitch);
            }

            png_read_image(file_reader.png_ptr, row_pointers.data());

            return result;
        }

        bool save_png(FILE* fp, const std::unique_ptr<gli::texture>& texture, pix_type type,
                      size_t level, size_t layer, size_t face)
        {
            struct file_writer
            {
                png_structp png_ptr = nullptr;
                png_infop info_ptr = nullptr;

                ~file_writer()
                {
                    if (png_ptr != nullptr)
                    {
                        png_destroy_write_struct(&png_ptr, &info_ptr);
                    }
                }
            };

            file_writer png_writer;
            png_writer.png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
            if (!png_writer.png_ptr)
            {
                return false;
            }

            png_writer.info_ptr = png_create_info_struct(png_writer.png_ptr);
            if (!png_writer.info_ptr)
            {
                return false;
            }

            if (setjmp(png_jmpbuf(png_writer.png_ptr)))
            {
                return false;
            }

            png_init_io(png_writer.png_ptr, fp);

            if (setjmp(png_jmpbuf(png_writer.png_ptr)))
            {
                return false;
            }

            auto color_type = PNG_COLOR_TYPE_RGBA;
            switch(type)
            {
            case pix_type::gray:
                color_type = PNG_COLOR_TYPE_GRAY;
                break;
            case pix_type::rgb:
                color_type = PNG_COLOR_TYPE_RGB;
                break;
            case pix_type::rgba:
                break;
            }

            const auto& size = texture->extent(level);

            png_set_IHDR(png_writer.png_ptr, png_writer.info_ptr, static_cast<uint32_t> (size.x),
                         static_cast<uint32_t> (size.y), 8 /*bits per pixel*/, color_type, PNG_INTERLACE_NONE,
                         PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
            png_write_info(png_writer.png_ptr, png_writer.info_ptr);

            if (setjmp(png_jmpbuf(png_writer.png_ptr)))
            {
                return false;
            }

            auto pitch = bytes_per_pixel(type) * size.x;
            auto data = reinterpret_cast<const uint8_t*>(texture->data(layer, face, level));
            for (auto i = 0; i < size.y; i++)
            {
                png_write_row(png_writer.png_ptr, data + i * pitch);
            }

            if (setjmp(png_jmpbuf(png_writer.png_ptr)))
            {
                return false;
            }

            png_write_end(png_writer.png_ptr, nullptr);

            return true;
        }

        std::tuple<rect, bool> is_png(FILE* fp)
        {


            { //check file is it png.
                uint8_t png_header[8] = {0x00, };
                auto result = ::fread(png_header, sizeof(png_header), 1, fp);
                if (result <= 0)
                {
                    return {};
                }

                if (png_sig_cmp(png_header, 0, sizeof(png_header)))
                {
                    return {};
                }
            }

            png::file_reader file_reader;
            file_reader.png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, png::warrning_fn);
            if (!file_reader.png_ptr)
            {
                return {};
            }

            png_set_read_fn(file_reader.png_ptr, fp, png::read_fn);

            //png_init_io(file_reader.png_ptr, fp.get());
            png_set_sig_bytes(file_reader.png_ptr, 8);

            file_reader.info_ptr = png_create_info_struct(file_reader.png_ptr);
            if (!file_reader.info_ptr)
            {
                return {};
            }
            png_read_info(file_reader.png_ptr, file_reader.info_ptr);

            gfx::rect rect;
            rect.w = static_cast<int> (png_get_image_width(file_reader.png_ptr, file_reader.info_ptr));
            rect.h = static_cast<int> (png_get_image_height(file_reader.png_ptr, file_reader.info_ptr));

            return {rect, true};
        }
    }
}
