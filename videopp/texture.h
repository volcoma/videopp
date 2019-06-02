#pragma once

#include "color.h"
#include "pixel_type.h"
#include "flip_format.h"
#include "rect.h"

#include <memory>
#include <string>
#include <chrono>

namespace video_ctrl
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
            streaming,          // FBO (Frame Buffer Object), for drawing onto texture  
            pixmap,             // x11 pixmap (X11 optimized texture surface)           
            target              // Can be updated using update_texture()                
        };
        
        /// Texture wrapping styles                     
        enum class wrap_type
        {
            wrap_clamp,         // Clamp texture to edges, avoiding color bleeding or repetition    
            wrap_repeat,        // You can repeat the texture infinitely                            
            wrap_mirror         // Each repeat of texture will be mirrored                          
        };

        /// Texture interpolation styles                
        enum class interpolation_type
        {   
            interpolate_none,   // A point filter, no interpolation whatsoever  
            interpolate_linear  // Linear interpolation                         
        };

        ~texture();

        // Use it if you have only streaming texture
        bool clear(const color &color);

        // Use it if you have only target texture   
        bool update(const point &point, const surface &surface);
        bool update(const rect &rect, pix_type pix_format, const uint8_t *buffer);

        // Use it if you have only FBO texture      
        bool read_pixels(const rect &rect, pix_type type, char *buffer);

        /* * * * * * * * * * * * * * * * * * * * * *
         *  generate_mipmap() generate smaller textures with high quality and use them when we have scaling
         *  use only for format_type::target
         * * * * * * * * * * * * * * * * * * * * * */
        bool generate_mipmap() noexcept;

        // Some encapsulation                                                                               
        inline const rect& get_rect() const { return rect_; }
        inline uint32_t get_id() const { return texture_; }
        inline pix_type get_pix_type() const { return pixel_type_; }
    private:
        friend class renderer;
        const renderer &rend_;

        uint32_t texture_ = 0;
        uint32_t fbo_ = 0;
        uint64_t glx_pixmap_ = 0;

        pix_type pixel_type_ = pix_type::rgb;
        format_type format_type_ = format_type::target;

        rect rect_ {};
        bool generated_mipmap_ = false;

        texture(const renderer &rend) noexcept;
        texture(const renderer &rend, int width, int height, pix_type pixel_type, format_type format_type);
        texture(const renderer &rend, const surface &surface);

        bool load_from_file(const std::string &path) noexcept;
        bool create_from_surface(const surface &surface) noexcept;
        bool check_for_error(const std::string &function_name) const noexcept;
        int32_t get_opengl_pixel_format(const pix_type &pixel_type) const;

        inline uint32_t get_FBO() const { return fbo_; }
    };

    using texture_ptr = std::shared_ptr<texture>;
    using texture_weak_ptr = std::weak_ptr<texture>;
}
