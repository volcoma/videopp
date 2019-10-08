#pragma once

#include "color.h"
#include "pixel_type.h"
#include "flip_format.h"
#include "rect.h"

#include <memory>
#include <string>
#include <vector>

namespace video_ctrl
{
    class texture;

    class surface
    {
    public:
        surface(const std::string &file_name);
        surface(int width, int height, pix_type type);
        surface(const uint8_t *buffer, int width, int height, pix_type type);
        surface(std::vector<uint8_t>&& buffer, int width, int height, pix_type type);

        surface() = delete;
        surface(const surface &other) = default;
        surface(surface &&other) noexcept = default;
        ~surface();

        int get_width() const;
        int get_height() const;
        const rect& get_rect() const { return rect_; }
        int get_bytes_per_pixel() const; // byte per pixel
        pix_type get_type() const;

        // position from pixel array
        std::pair<point, bool> find_pixel(const color &color, point start = {}) const noexcept;

        bool set_pixel(point pos, const color &color) noexcept;
        color get_pixel(const point& point) const noexcept;
        void fill(const color &color) noexcept;

        std::unique_ptr<surface> convert_to(pix_type type);

        surface subsurface(const rect &rct);
        const uint8_t* get_pixels() const;

        void flip(flip_format flip);

        //copy functions
        bool copy_from(const surface &src_surf, const rect &src_rect, const point &dest_point);
    private:
        friend class texture;

        enum class surface_type
        {
            raw,
            compressed
        };

        void flip_horizontally();
        void flip_vertically();


        rect rect_;
        surface_type surface_type_ = surface_type::raw;
        pix_type type_ = pix_type::rgb;
        std::vector<uint8_t> data_;
    };

    using surface_ptr = std::unique_ptr<surface>;
    using surface_shared_ptr = std::shared_ptr<surface>;
}
