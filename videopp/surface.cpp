#include "surface.h"
#include "logger.h"
#include <algorithm>

//#include <libpng16/png.h>

namespace video_ctrl
{
    surface::~surface() = default;

    surface::surface(const std::string &file_name)
    {
        if (load_png(file_name))
        {
            return;
        }
        else if (load_dds(file_name))
        {
            return;
        }

        throw video_ctrl::exception("Cannot create surface from file " + file_name);
    }

    surface::surface(int width, int height, pix_type type)
        : rect_(0, 0, width, height),
          type_(type),
          data_(size_t(width * height * get_bytes_per_pixel()))
    {
    }

    surface::surface(const uint8_t *buffer, int width, int height, pix_type type)
        : rect_(0, 0, width, height),
          type_(type),
          data_(buffer, buffer + static_cast<uint64_t> (width * height * get_bytes_per_pixel()))
    {
    }

    surface::surface(std::vector<uint8_t>&& buffer, int width, int height, pix_type type)
        : rect_(0, 0, width, height),
          type_(type),
          data_(std::move(buffer))
    {

    }


    int surface::get_width() const
    {
        return rect_.w;
    }

    int surface::get_height() const
    {
        return rect_.h;
    }

    int surface::get_bytes_per_pixel() const
    {
        return static_cast<int> (type_);
    }

    pix_type surface::get_type() const
    {
        return type_;
    }

    std::pair<point, bool> surface::find_pixel(const video_ctrl::color& color, point start) const noexcept
    {
        if (surface_type_ != surface_type::raw)
        {
            return {{}, false};
        }

        for (; start.y < rect_.h; start.y++)
        {
            for (; start.x < rect_.w; start.x++)
            {
                if (color == get_pixel(start))
                {
                    return {start, true};
                }
            }
            start.x = 0;
        }

        return {{}, false};
    }

    bool surface::set_pixel(point pos, const video_ctrl::color& color) noexcept
    {
        if (surface_type_ != surface_type::raw)
        {
            return false;
        }

        if (pos.x >= rect_.w || pos.y >= rect_.h || pos.x < 0 || pos.y < 0)
        {
            return false;
        }

        auto pitch = rect_.w * get_bytes_per_pixel();
        uint8_t *pixel_start = data_.data() + pos.x * get_bytes_per_pixel() + pos.y * pitch;

        switch (type_)
        {
        case pix_type::gray:
            *pixel_start = static_cast<uint8_t>((color.r + color.g + color.b) / 3);
            break;
        case pix_type::rgb:
            *pixel_start = color.r;
            *(pixel_start+1) = color.g;
            *(pixel_start+2) = color.b;
            break;
        case pix_type::rgba:
            *pixel_start = color.r;
            *(pixel_start+1) = color.g;
            *(pixel_start+2) = color.b;
            *(pixel_start+3) = color.a;
            break;
        }

        return true;
    }

    color surface::get_pixel(const point &point) const noexcept
    {
        if (surface_type_ != surface_type::raw)
        {
            return {0,0,0,0};
        }

        if (point.x >= rect_.w || point.y >= rect_.h)
        {
            return {0,0,0,0};
        }

        const uint8_t *pixel = data_.data();
        auto pitch = get_bytes_per_pixel() * rect_.w;
        const uint8_t *search_pixel_start = pixel + point.x * get_bytes_per_pixel() + point.y * pitch;

        switch (type_)
        {
        case pix_type::gray:
            return {*search_pixel_start, *search_pixel_start, *search_pixel_start};
        case pix_type::rgb:
            return {*search_pixel_start, *(search_pixel_start+1), *(search_pixel_start+2)};
        case pix_type::rgba:
            return {*search_pixel_start, *(search_pixel_start+1), *(search_pixel_start+2), *(search_pixel_start+3)};
        }
        
        return {0,0,0,0};
    }

    void surface::fill(const color &color) noexcept
    {
        if (surface_type_ != surface_type::raw)
        {
            return;
        }

        switch (type_)
        {
        case pix_type::gray:
            std::fill(data_.begin(), data_.end(), color.r);
            break;
        case pix_type::rgb:
            for (uint64_t i = 0; i < data_.size(); i += 3)
            {
                data_.at(i) = color.r;
                data_.at(i+1) = color.g;
                data_.at(i+2) = color.b;
            }
            break;
        case pix_type::rgba:
            for (uint64_t i = 0; i < data_.size(); i += 4)
            {
                data_.at(i) = color.r;
                data_.at(i+1) = color.g;
                data_.at(i+2) = color.b;
                data_.at(i+3) = color.a;
            }
            break;
        }
    }

    std::unique_ptr<surface> surface::convert_to(pix_type type)
    {
        if (surface_type_ != surface_type::raw)
        {
            throw video_ctrl::exception("Cannot convert compressed surface.");
        }

        if (type == type_)
        {
            return std::make_unique<surface> (data_.data(), rect_.w, rect_.h, type);
        }

        auto result = std::make_unique<surface>(rect_.w, rect_.h, type);

        video_ctrl::point copy_point {0, 0};
        for (copy_point.y = 0; copy_point.y < rect_.h; copy_point.y++)
        {
            for (copy_point.x = 0; copy_point.x < rect_.w; copy_point.x++)
            {
                result->set_pixel(copy_point, get_pixel(copy_point));
            }
        }

        return result;
    }

    surface surface::subsurface(const rect &rct)
    {
        if (surface_type_ != surface_type::raw)
        {
            throw video_ctrl::exception("Cannot copy compress surface.");
        }

        surface result(rct.w, rct.h, pix_type::rgba);

        if (!result.copy_from(*this, rct, {0, 0}))
        {
            throw video_ctrl::exception("Cannot copy to new subsurface.");
        }

        return result;
    }

    const uint8_t* surface::get_pixels() const
    {
        if (surface_type_ == surface_type::raw)
        {
            return data_.data();
        }
        else
        {
            return data_.data();
        }
    }

    uint8_t* surface::get_buffer()
    {
        if (surface_type_ == surface_type::raw)
        {
            return data_.data();
        }
        else
        {
            return data_.data();
        }
    }

    void surface::flip(flip_format flip)
    {
        if (surface_type_ != surface_type::raw)
        {
            return;
        }

        switch (flip)
        {
        case none:
            return;
        case flip_format::vertical:
            flip_vertically();
            break;
        case flip_format::horizontal:
            flip_horizontally();
            break;
        case flip_format::both:
            flip_vertically();
            flip_horizontally();
            break;
        }
    }

    bool surface::copy_from(const surface &src_surf, const rect &src_rect, const point &dest_point)
    {
        if (surface_type_ != surface_type::raw)
        {
            return false;
        }

        if (src_rect.x + src_rect.w > src_surf.rect_.w || src_rect.y + src_rect.h > src_surf.rect_.h)
        {
            return false;
        }

        if (src_rect.w > rect_.w - dest_point.x || src_rect.h > rect_.h - dest_point.y)
        {
            return false;
        }

        if (src_surf.type_ != type_)
        {
            point src_point {};
            point dst_point {};
            for (src_point.y = src_rect.y, dst_point.y = dest_point.y; src_point.y < src_rect.h; src_point.y++, dst_point.y++)
            {
                for (src_point.x = src_rect.x, dst_point.x = dest_point.x; src_point.x < src_rect.w; src_point.x++, dst_point.x++)
                {
                    auto col = src_surf.get_pixel(src_point);
                    if (col.a == 0)
                    {
                        col = video_ctrl::color::black;
                    }
                    if (!set_pixel(dst_point, col))
                    {
                        log("Cannot copy surface.");
                        return false;
                    }
                }
            }

            return true;
        }

        const uint8_t* src_pixels = src_surf.get_pixels();
        uint8_t* dest_pixels = data_.data();

        auto src_bpp = src_surf.get_bytes_per_pixel();
        auto dest_bpp = get_bytes_per_pixel();

        auto src_rect_pitch = static_cast<uint32_t> (src_rect.w * src_bpp);
        auto src_pitch = src_surf.rect_.w * src_bpp;
        auto dest_pitch = rect_.w * dest_bpp;

        src_pixels += src_rect.x * src_bpp + src_rect.y * src_pitch;
        dest_pixels += dest_point.x * dest_bpp + dest_point.y * dest_pitch;

        for (auto i = 0; i < src_rect.h; i++)
        {
            std::memcpy(dest_pixels, src_pixels, src_rect_pitch);
            src_pixels += src_pitch;
            dest_pixels += dest_pitch;
        }

        return true;
    }

    bool surface::load_png(const std::string &file_name)
    {
//        std::unique_ptr<FILE, file_deleter> fp(fopen(file_name.c_str(), "rb"));
//        if (!fp)
//        {
//            return false;
//        }

//        { //check file is it png.
//            uint8_t png_header[8] = {0x00, };
//            auto result = fread(png_header, sizeof(png_header), 1, fp.get());
//            if (result <= 0)
//            {
//                return false;
//            }

//            if (png_sig_cmp(png_header, 0, sizeof(png_header)))
//            {
//                return false;
//            }
//        }

//        struct file_reader
//        {
//            png_structp png_ptr = nullptr;
//            png_infop info_ptr = nullptr;

//            ~file_reader()
//            {
//                if (png_ptr != nullptr)
//                {
//                    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
//                }
//            }
//        };

//        const auto warrning_fn = [](png_structp, png_const_charp) {

//        };

//        file_reader file_reader;

//        file_reader.png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, warrning_fn);

//        if (!file_reader.png_ptr)
//        {
//            return false;
//        }

//        png_init_io(file_reader.png_ptr, fp.get());
//        png_set_sig_bytes(file_reader.png_ptr, 8);

//        file_reader.info_ptr = png_create_info_struct(file_reader.png_ptr);
//        if (!file_reader.info_ptr)
//        {
//            return false;
//        }
//        png_read_info(file_reader.png_ptr, file_reader.info_ptr);

//        rect_.w = static_cast<int> (png_get_image_width(file_reader.png_ptr, file_reader.info_ptr));
//        rect_.h = static_cast<int> (png_get_image_height(file_reader.png_ptr, file_reader.info_ptr));
//        auto color_type = png_get_color_type(file_reader.png_ptr, file_reader.info_ptr);
//        auto bit_depth = png_get_bit_depth(file_reader.png_ptr, file_reader.info_ptr);

//        if (bit_depth == 16 /* bits per color */)
//        {
//            // set strip for 16 bits to 8 bits. We need only 8bits per color quality.
//            png_set_strip_16(file_reader.png_ptr);
//        }

//        switch (color_type)
//        {
//        case PNG_COLOR_TYPE_GRAY:
//            if (bit_depth < 8 /* bits per color */)
//            {
//                png_set_expand_gray_1_2_4_to_8(file_reader.png_ptr);
//            }
//            type_ = pix_type::gray;
//            break;

//        case PNG_COLOR_TYPE_GRAY_ALPHA:
//            // this type is always 8 or 16 bits but we already strip 16 bits
//            png_set_gray_to_rgb(file_reader.png_ptr);
//            if (png_get_valid(file_reader.png_ptr, file_reader.info_ptr, PNG_INFO_tRNS))
//            {
//                png_set_tRNS_to_alpha(file_reader.png_ptr);
//            }
//            type_ = pix_type::rgba;
//            break;

//        case PNG_COLOR_TYPE_RGB:
//            // set alpha for opengl texture create optimization
//            //log("WARNING: Input file " + file_name + " is RGB format. Please convert it to RGBA format.");
//            png_set_filler(file_reader.png_ptr, 0xFF, PNG_FILLER_AFTER);
//            type_ = pix_type::rgba;
//            break;

//        case PNG_COLOR_TYPE_RGBA:
//            type_ = pix_type::rgba;
//            break;

//        case PNG_COLOR_TYPE_PALETTE:
//            //log("WARNING: Input file " + file_name +
//            //    " is COLOR PALETTE format. Please convert it to RGBA format.");
//            png_set_palette_to_rgb(file_reader.png_ptr);
//            if (png_get_valid(file_reader.png_ptr, file_reader.info_ptr, PNG_INFO_tRNS))
//            {
//                png_set_tRNS_to_alpha(file_reader.png_ptr);
//            }
//            else
//            {
//                png_set_filler(file_reader.png_ptr, 0xFF, PNG_FILLER_AFTER);
//            }
//            type_ = pix_type::rgba;
//            break;

//        default:
//            return false;
//        }

//        png_read_update_info(file_reader.png_ptr, file_reader.info_ptr);
//        auto pitch = png_get_rowbytes(file_reader.png_ptr, file_reader.info_ptr);

//        std::vector<uint8_t*> row_pointers;
//        row_pointers.reserve(static_cast<uint32_t> (rect_.h));
//        data_.resize(pitch * static_cast<uint32_t> (rect_.h));

//        for (uint32_t i = 0; i < static_cast<uint32_t> (rect_.h); i++)
//        {
//            //png_read_row(file_reader.png_ptr, data_.data() + i * pitch, nullptr);
//            row_pointers.push_back(data_.data() + i * pitch);
//        }

//        png_read_image(file_reader.png_ptr, row_pointers.data());
//        surface_type_ = surface_type::raw;

//        return true;
        return false;
    }

    bool surface::load_dds(const std::string& file_name)
    {
        return false;
    }

    void surface::flip_horizontally()
    {
        auto half_width = static_cast<uint32_t>(rect_.w / 2);
        uint8_t* pixels = data_.data();
        auto pitch = get_bytes_per_pixel() * rect_.w;
        auto bpp = get_bytes_per_pixel();
        for (size_t y = 0; y < static_cast<size_t> (rect_.h); y++)
        {
            uint8_t *left_pixel = pixels + y * static_cast<uint32_t> (pitch);
            uint8_t *right_pixel = pixels + (y + 1) * static_cast<uint32_t> (pitch) - bpp;

            for (size_t x = 0; x < half_width; x++)
            {
                std::swap_ranges(left_pixel, left_pixel + bpp, right_pixel);

                left_pixel += bpp;
                right_pixel -= bpp;
            }
        }
    }

    void surface::flip_vertically()
    {
        uint8_t* pixels = data_.data();
        auto half_height = static_cast<uint32_t>(rect_.h / 2);
        auto pitch = static_cast<uint32_t> (rect_.w * get_bytes_per_pixel());
        uint8_t *top_pixels = pixels;
        uint8_t *bottom_pixels = pixels + pitch * static_cast<uint32_t> (rect_.h - 1);

        for (size_t y = 0; y < half_height; y++)
        {
            std::swap_ranges(top_pixels, top_pixels + pitch, bottom_pixels);

            top_pixels += pitch;
            bottom_pixels -= pitch;
        }

    }

    void surface::file_deleter::operator()(FILE *fp)
    {
        if (fp != nullptr)
        {
            fclose(fp);
        }
    }
}
