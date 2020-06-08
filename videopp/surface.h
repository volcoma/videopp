#pragma once

#include "color.h"
#include "pixel_type.h"
#include "flip_format.h"
#include "rect.h"

#include <memory>
#include <string>
#include <vector>

namespace gli
{
    class texture;
}

namespace gfx
{
    class texture;

    class surface
    {
    public:
        static std::tuple<rect, bool> is_surface_compatible(const std::string& file_name);

        surface(const std::string &file_name);
        surface(const uint8_t* file_buffer, size_t file_buffer_size);
        surface(int width, int height, pix_type type);
        surface(const uint8_t *buffer, int width, int height, pix_type type);
        surface(std::vector<uint8_t>&& buffer, int width, int height, pix_type type);

        surface(surface&& other) noexcept;
        ~surface();

        size_t get_layers() const noexcept;
        size_t get_levels() const noexcept;
        size_t get_faces() const noexcept;

        bool save_to_file(const std::string &file_name, size_t level = 0, size_t layer = 0, size_t face = 0);

        int get_width(size_t level = 0) const noexcept;
        int get_height(size_t level = 0) const noexcept;
        const rect& get_rect(size_t level = 0) const noexcept;
        int get_bytes_per_pixel() const noexcept; // byte per pixel
        pix_type get_type() const noexcept;

        // position from pixel array
        std::pair<point, bool> find_pixel(const color &color, const rect& area, size_t level = 0) const noexcept;
        std::pair<point, bool> find_pixel_with_alpha(const rect& area, size_t level = 0) const noexcept;
        std::pair<point, bool> find_pixel_with_alpha_reverse(const rect& area, size_t level = 0) const noexcept;

        bool set_pixel(const point& pos, const color &color, size_t level = 0, size_t layer = 0, size_t face = 0) noexcept;
        color get_pixel(const gfx::point &point, size_t level = 0) const noexcept;
        void fill(const color &color) noexcept;

        //std::unique_ptr<surface> convert_to(pix_type type);

        const uint8_t* get_data(size_t level = 0, size_t layer = 0, size_t face = 0) const;
        const std::unique_ptr<gli::texture>& get_native_handle() const noexcept;

        void flip(flip_format flip);

        size get_block_extent() const;

        //copy functions
        std::unique_ptr<surface> create_empty(const size& new_surface_size = {}) const;
        bool copy_from(const surface &src_surf, const rect &src_rect, const point &dest_point,
                       size_t src_level = 0, size_t src_layer = 0, size_t src_face = 0,
                       size_t dst_level = 0, size_t dst_layer = 0, size_t dst_face = 0);
    private:
        surface() = default;

        friend class texture;

        bool load_png(const std::string &file_name);
        bool load_png(const uint8_t* file_buffer, size_t file_buffer_size);
        bool load_png(FILE* file);
        bool load_dds(const std::string &file_name);
        bool load_dds(const uint8_t* file_buffer, size_t file_buffer_size);

        std::unique_ptr<gli::texture> gli_surface_;
        std::vector<gfx::rect> rects_ {};

        pix_type type_ = pix_type::rgba;
        bool compressed_ {};
        bool had_alpha_pixels_originally_{};
    };

    using surface_ptr = std::unique_ptr<surface>;
    using surface_shared_ptr = std::shared_ptr<surface>;
}
