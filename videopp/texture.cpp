#include "texture.h"

#include "renderer.h"
#include "surface.h"
#include "utils.h"


namespace video_ctrl
{


    texture::texture(const renderer &rend) noexcept
        : rend_(rend)
    {
    }

    /// Texture constructor
    ///     @param rend - renderer interface
    ///     @param width - texture width in pixels
    ///     @param height - texture height in pixels
    ///     @param pixel_type - type of pixel data
    ///     @param format type - texture usage type
    texture::texture(const renderer &rend, int width, int height, pix_type pixel_type, texture::format_type format_type)
        : rend_(rend)
        , pixel_type_(pixel_type)
        , format_type_(format_type)
        , rect_(0,0, width, height)
    {
        if (!rend_.set_current_context())
        {
            throw video_ctrl::exception("Cannot set current context!");
        }

        if (texture::format_type::pixmap == format_type_)
        {

        }

        // Generate the OGL texture ID
        int pixel_format = get_opengl_pixel_format(pixel_type);
        gl_call(glGenTextures(1, &texture_));
        rend_.set_texture(texture_, 0, wrap_type::wrap_clamp, interpolation_type::interpolate_linear);

        // Upload data to VRAM
        if (texture::format_type::target == format_type_)
        {
            gl_call(glTexImage2D(GL_TEXTURE_2D, 0, pixel_format, width, height, 0, static_cast<GLenum> (pixel_format), GL_UNSIGNED_BYTE, nullptr));
        }
        else if (texture::format_type::streaming == format_type_)
        {
            gl_call(glTexImage2D(GL_TEXTURE_2D, 0, pixel_format, width, height, 0, static_cast<GLenum> (pixel_format), GL_UNSIGNED_BYTE, nullptr));

            // We're building a framebuffer object
            gl_call(glGenFramebuffers(1, &fbo_));
            gl_call(glBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo_));
            gl_call(glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture_, 0));

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
            gl_call(glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0));
            if(status != GL_FRAMEBUFFER_COMPLETE_EXT)
            {
                throw video_ctrl::exception("Cannot create FBO. GL ERROR CODE: " + std::to_string(status));
            }
        }

        rend_.reset_texture(0);
    }

    /// Texture constructor from a surface
    ///     @param rend - renderer interface
    ///     @param surface - the surface to copy pixels from
    texture::texture(const renderer &rend, const surface &surface)
        : rend_(rend)
        , rect_(0, 0, surface.get_width(), surface.get_height())
    {
        if (!create_from_surface(surface))
        {
            throw video_ctrl::exception("Cannot create texture from surface.");
        }
    }

    texture::~texture()
    {
        if (fbo_ > 0)
        {
            // Destroy FBO
            gl_call(glDeleteFramebuffers(1, &fbo_));
            fbo_ = 0;
        }
        
        if (texture_ > 0)
        {
            // Destroy texture
            gl_call(glDeleteTextures(1, &texture_));
            texture_ = 0;
        }
    }

    /// Clear the texture. Works only for FBO textures
    ///     @param color - color to clear to
    ///     @return true on success
    bool texture::clear(const color &color)
    {
        if (format_type_ != format_type::streaming)
        {
            return false;
        }

        rend_.clear_fbo(fbo_, color);
        return true;
    }

    /// Load from a file
    ///     @param path - the filename
    ///     @return true on success
    bool texture::load_from_file(const std::string &path) noexcept
    {
        try
        {
            surface surf(path);

            if (!create_from_surface(surf))
            {
                return false;
            }

            rect_.w = surf.get_width();
            rect_.h = surf.get_height();
        }
        catch (exception &e)
        {
            log("ERROR: " + std::string(e.what()));
            return false;
        }

        return true;
    }

    /// Copy from a surface
    ///     @param surface - preloaded surface
    ///     @return true on success
    bool texture::create_from_surface(const surface &surface) noexcept
    {
        if (!rend_.set_current_context())
        {
            return false;
        }

        gl_call(glGenTextures(1, &texture_));

        if (surface.surface_type_ == surface::surface_type::raw)
        {
            rend_.set_texture(texture_, 0, wrap_type::wrap_clamp, interpolation_type::interpolate_linear);

            GLenum format = static_cast<GLenum> (get_opengl_pixel_format(surface.get_type()));
            int32_t internal_format = GL_RGBA;
            pixel_type_ = pix_type::rgba;

            switch(surface.get_type())
            {
                case pix_type::gray:
                    internal_format = format;
                    pixel_type_ = surface.get_type();
                break;
                default:

                break;
            }
            gl_call(glTexImage2D(GL_TEXTURE_2D, 0, internal_format, surface.get_width(), surface.get_height(), 0, format,
                         GL_UNSIGNED_BYTE, surface.get_pixels()));

            rend_.reset_texture(0);
        }
        else if (surface.surface_type_ == surface::surface_type::compress)
        {
            return false;
        }
        else
        {
            return false;
        }

        return true;
    }

    /// Hat pixel format to opengl pixel format converter
    ///     @param pixel_type - hat format to convert
    ///     @return the opengl format
    int32_t texture::get_opengl_pixel_format(const pix_type &pixel_type) const
    {
        int32_t pixel_format = GL_RGBA;
        switch (pixel_type)
        {
            case pix_type::gray:
                pixel_format = GL_RED;
                break;
            case pix_type::rgb:
                pixel_format = GL_RGB;
                break;
            case pix_type::rgba:
                pixel_format = GL_RGBA;
                break;
        }

        return pixel_format;
    }

    /// Upload new data to VRAM buffer (from surface)
    ///     @param point - offset inside texture pixelspace
    ///     @param surface - surface to copy from
    ///     @return true on success
    bool texture::update(const point &point, const surface &surface)
    {
        rect rect { point.x, point.y, surface.get_width(), surface.get_height() };
        return update(rect, surface.get_type(), surface.get_pixels());
    }

    /// Upload new data to VRAM buffer (from raw data)
    ///     @param rect - rectangle to copy
    ///     @param pix_format - hat pixel format
    ///     @param buffer - data to copy from
    ///     @return true on success
    bool texture::update(const rect &rect, pix_type pix_format, const uint8_t *buffer)
    {
        if (format_type_ != format_type::target)
        {
            return false;
        }

        const auto format = static_cast<GLenum> (get_opengl_pixel_format(pix_format));
        rend_.set_texture(texture_, 0, wrap_type::wrap_repeat, interpolation_type::interpolate_linear);
        gl_call(glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.w, rect.h, format, GL_UNSIGNED_BYTE, buffer));
        rend_.reset_texture(0);
        return true;
    }

    /// Read pixels from the texture (from VRAM - slow!)
    ///     @param rect - rectangle to read
    ///     @param type - hat pixel format
    ///     @param buffer - [out] data goes here
    ///     @return true on success
    bool texture::read_pixels(const rect &rect, pix_type type, char *buffer)
    {
        if (!rend_.set_current_context())
        {
            return false;
        }

        gl_call(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_));
        gl_call(glReadPixels(rect.x, rect.y, rect.w, rect.h, type == pix_type::rgba ? GL_BGRA : GL_BGR, GL_UNSIGNED_BYTE, buffer));
        rend_.set_old_framebuffer();
        return true;
    }

    /// Generate mipmaps for the texture. Works only for target textures (don't know why)
    ///     @return true on success
    bool texture::generate_mipmap() noexcept
    {
        if (format_type_ != format_type::target || generated_mipmap_ || !rend_.set_current_context())
        {
            return false;
        }

        gl_call(glBindTexture(GL_TEXTURE_2D, texture_));
        gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST));
        gl_call(glGenerateMipmap(GL_TEXTURE_2D));
        gl_call(glBindTexture(GL_TEXTURE_2D, 0));

        generated_mipmap_ = true;
        return generated_mipmap_;
    }
}
