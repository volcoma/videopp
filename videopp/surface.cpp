#include "surface.h"
#include "logger.h"
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "image/stb_image.h"

namespace video_ctrl
{
    surface::~surface() = default;

    surface::surface(const std::string &file_name)
    {
        int w = 0,h = 0,n = 0;
        unsigned char* data = stbi_load(file_name.c_str(), &w, &h, &n, 0);
        if(!data)
        {
            throw video_ctrl::exception("Cannot create surface from file " + file_name);

        }
        switch(n)
        {
            case 1:
                type_ = pix_type::gray;
            break;
            case 3:
                type_ = pix_type::rgb;
            break;
            case 4:
                type_ = pix_type::rgba;
            break;
            default:
                stbi_image_free(data);
                throw video_ctrl::exception("Cannot create surface from file " + file_name);
            break;
        }
        rect_= rect(0, 0, w, h);
        data_ = std::vector<uint8_t>(data, data + (w * h * n));
        stbi_image_free(data);
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
                        col = video_ctrl::color::black();
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
