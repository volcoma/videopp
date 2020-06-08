#pragma once

#include "color.h"
#include "context.h"
#include "pixel_type.h"
#include "flip_format.h"
#include "rect.h"

#include <memory>
#include <string>
#include <chrono>

namespace gfx
{
    /// Predefinitions
    class renderer;
    class surface;

    /// OpenGL texture/FBO wrapper
    class texture
    {
    public:
        /// Supported texture types
        enum class format_type
        {
            streaming,              // FBO (Frame Buffer Object), for drawing onto texture
            pixmap,                 // x11 pixmap (X11 optimized texture surface)
            target,                 // Can be updated using update_texture()
            compress,               // Compressed texture format
        };

        /// Texture wrapping styles
        enum class wrap_type
        {
            wrap_clamp,         // Clamp texture to edges, avoiding color bleeding or repetition
            wrap_repeat,        // You can repeat the texture infinitely
            wrap_mirror,        // Each repeat of texture will be mirrored
            count
        };

        /// Texture interpolation styles
        enum class interpolation_type
        {
            interpolate_none,   // A point filter, no interpolation whatsoever
            interpolate_linear, // Linear interpolation
            count
        };

        ~texture();

        // Use it if you have only target texture
        bool update(const point &point, const surface &surface);
        bool update(const rect &rect, pix_type pix_format, const uint8_t *buffer, const size_t level = 0);
        bool update(const point &point, const surface& surface,
                    size_t srcLevel, size_t srcFace, size_t srcLayer,
                    size_t dstLevel, size_t dstFace, size_t dstLayer);

        // Use it if you have only FBO texture
        bool read_pixels(const rect &rect, pix_type type, char *buffer);

        /* * * * * * * * * * * * * * * * * * * * * *
             *  generate_mipmap() generate smaller textures with high quality and use them when we have scaling
             *  use only for format_type::target
             * * * * * * * * * * * * * * * * * * * * * */
        bool generate_mipmap() noexcept;
        void set_default_blending_mode(blending_mode mode);
        // Some encapsulation
        inline const rect& get_rect() const { return rect_; }
        inline uint32_t get_id() const { return texture_; }
        inline const pixmap& get_pixmap() const { return pixmap_; }
        const void* get_pixmap_native_handle() const;
//        inline uint64_t get_glx_pixmap() const { return glx_pixmap_; }
        inline pix_type get_pix_type() const { return pixel_type_; }
        inline wrap_type get_wrap_type() const { return wrap_type_; }
        inline interpolation_type get_interp_type() const { return interp_type_; }
        inline blending_mode get_default_blending_mode() const { return blending_; }
        inline format_type get_format_type() const { return format_type_; }
    private:
        friend class renderer;
        const renderer &rend_;

        uint32_t texture_ = 0;
        uint32_t fbo_ = 0;
        pixmap pixmap_ {};

        pix_type pixel_type_ = pix_type::rgb;

        format_type format_type_ = format_type::target;
        wrap_type wrap_type_ = wrap_type::wrap_clamp;
        interpolation_type interp_type_ = interpolation_type::interpolate_linear;
        blending_mode blending_ = blending_mode::blend_normal;

        rect rect_ {};
        bool generated_mipmap_ {false};

        texture(const renderer &rend) noexcept;
        texture(const renderer &rend, int width, int height, pix_type pixel_type, format_type format_type);
        texture(const renderer &rend, const std::tuple<size, pix_type, format_type>& data);
        texture(const renderer &rend, const surface &surface, bool empty = false);
        texture(const renderer& rend, const surface& surface, size_t level_id, size_t layer_id, size_t face_id);

        bool setup_texparameters();

        bool load_from_file(const std::string &path) noexcept;
        bool create_from_surface(const surface &surface, bool empty, size_t start_level_id, size_t start_layer_id,
                                 size_t start_face_id, size_t levels_count, size_t layers_cout, size_t faces_count) noexcept;

        inline uint32_t get_FBO() const { return fbo_; }
    };

    using texture_ptr = std::shared_ptr<texture>;
    using texture_weak_ptr = std::weak_ptr<texture>;


    /// A non-owning texture representation
    struct texture_view
    {
        texture_view() = default;
        texture_view(const texture_ptr& texture) noexcept;
        texture_view(const texture_ptr& texture, texture::wrap_type wrap, texture::interpolation_type interp) noexcept;
        texture_view(std::uint32_t tex_id, std::uint32_t tex_width = 0, std::uint32_t tex_height = 0) noexcept;
        /// Check if texture representation is valid
        inline bool is_valid() const noexcept
        {
            return id != 0;
        }
        inline operator bool() const
        {
            return is_valid();
        }

        /// Compare textures by id and size
        bool operator==(const texture_view& rhs) const noexcept;
        void* get() const noexcept;

        /// Create a texture representation from a loaded texture
        static texture_view create(const texture_ptr& texture) noexcept;
        static texture_view create(std::uint32_t tex_id, std::uint32_t tex_width = 0, std::uint32_t tex_height = 0) noexcept;

        std::uint32_t width = 0;    // texture width
        std::uint32_t height = 0;   // texture height
        std::uint32_t id = 0;       // internal VRAM texture id
        texture::wrap_type wrap_type = texture::wrap_type::wrap_clamp;
        texture::interpolation_type interp_type = texture::interpolation_type::interpolate_linear;
        blending_mode blending = blending_mode::blend_normal;
        pix_type format = pix_type::rgba;

        // reserved
        gfx::pixmap pixmap {};   // id extension.
        bool custom_sampler{false};
    };

    using texture_src_data = std::tuple<size, pix_type, texture::format_type>;
}
namespace std
{
    template<> struct hash<gfx::texture_view>
    {
        using argument_type = gfx::texture_view;
        using result_type = std::size_t;
        result_type operator()(argument_type const& s) const noexcept
        {
            uint64_t seed{0};
            utils::hash(seed, s.id, s.wrap_type, s.interp_type);
            return seed;
        }
    };
}
