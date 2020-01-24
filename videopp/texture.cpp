#include "texture.h"

#include "renderer.h"
#include "surface.h"
#include "detail/utils.h"


namespace gfx
{

    namespace
    {
        std::tuple<int32_t, int32_t> get_opengl_pixel_format(const pix_type &pixel_type)
        {
            switch (pixel_type)
            {
                case pix_type::red:
                    return {GL_RED, GL_R8};
                case pix_type::rgb:
                    return {GL_RGB, GL_RGB8};
                case pix_type::rgba:
                    return {GL_RGBA, GL_RGBA8};
                default:
                    return {GL_RGBA, GL_RGBA8};
            }
        }

    }

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
            throw gfx::exception("Cannot set current context!");
        }

        if(pixel_type == pix_type::rgb)
        {
            blending_ = blending_mode::blend_none;
        }

        // Generate the OGL texture ID
        int32_t format{};
        int32_t internal_format{};
        std::tie(format, internal_format) = get_opengl_pixel_format(pixel_type);
        gl_call(glGenTextures(1, &texture_));
        gl_call(glBindTexture(GL_TEXTURE_2D, texture_));

        // Upload data to VRAM
        if (texture::format_type::target == format_type_)
        {
            gl_call(glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, static_cast<GLenum> (format), GL_UNSIGNED_BYTE, nullptr));
        }
        else if (texture::format_type::streaming == format_type_)
        {
            gl_call(glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, static_cast<GLenum> (format), GL_UNSIGNED_BYTE, nullptr));

            // We're building a framebuffer object
            gl_call(glGenFramebuffers(1, &fbo_));
            gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo_));
            gl_call(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0));

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            gl_call(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            if(status != GL_FRAMEBUFFER_COMPLETE)
            {
                throw gfx::exception("Cannot create FBO. GL ERROR CODE: " + std::to_string(status));
            }
        }

        setup_texparameters();

        gl_call(glBindTexture(GL_TEXTURE_2D, 0));
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
            throw gfx::exception("Cannot create texture from surface.");
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

    bool texture::setup_texparameters() const
    {
        // Pick the wrap mode for rendering
        GLint wmode = GL_REPEAT;
        switch(wrap_type_)
        {
        case wrap_type::mirror:
            wmode = GL_MIRRORED_REPEAT;
            break;
        case wrap_type::repeat:
            wmode = GL_REPEAT;
            break;
        case wrap_type::clamp:
        default:
            wmode = GL_CLAMP_TO_EDGE;
            break;
        }
        gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wmode));
        gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wmode));

        // Pick the interpolation mode for rendering
        GLint imode = GL_LINEAR;
        switch(interp_type_)
        {
        case interpolation_type::nearest:
            imode = GL_NEAREST;
            break;
        case interpolation_type::linear:
        default:
            imode = GL_LINEAR;
            break;
        }
        gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, imode));
        gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, imode));

        return true;
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
            pixel_type_ = surface.get_type();

            int32_t format{};
            int32_t internal_format{};
            std::tie(format, internal_format) = get_opengl_pixel_format(surface.get_type());

            if(pixel_type_ == pix_type::rgb)
            {
                blending_ = blending_mode::blend_none;
            }


            gl_call(glBindTexture(GL_TEXTURE_2D, texture_));
//            gl_call(glTexStorage2D(GL_TEXTURE_2D, 1, GLenum(internal_format), surface.get_width(), surface.get_height()));
//            gl_call(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface.get_width(), surface.get_height(),
//                            static_cast<GLenum> (format), GL_UNSIGNED_BYTE, surface.get_pixels()));
            gl_call(glTexImage2D(GL_TEXTURE_2D, 0, GLenum(internal_format), surface.get_width(), surface.get_height(), 0,
                            static_cast<GLenum> (format), GL_UNSIGNED_BYTE, surface.get_pixels()));
            if( !setup_texparameters() )
            {
                return false;
            }

            gl_call(glBindTexture(GL_TEXTURE_2D, 0));

            format_type_ = format_type::immutable;
        }
        else if (surface.surface_type_ == surface::surface_type::compressed)
        {
            return false;
        }
        else
        {
            return false;
        }

        return true;
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

        int32_t format{};
        int32_t internal_format{};
        std::tie(format, internal_format) = get_opengl_pixel_format(pix_format);

        gl_call(glBindTexture(GL_TEXTURE_2D, texture_));
        gl_call(glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.w, rect.h, static_cast<GLenum> (format), GL_UNSIGNED_BYTE, buffer));
        gl_call(glBindTexture(GL_TEXTURE_2D, 0));
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

        gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo_));
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


    texture_view::texture_view(const texture_ptr& texture) noexcept
    {
        *this = create(texture);
    }

    texture_view::texture_view(const texture_ptr &texture, texture::wrap_type wrap, texture::interpolation_type interp) noexcept
    {
        *this = create(texture);

        if(texture)
        {
            if( texture->get_wrap_type() != wrap || texture->get_interp_type() != interp )
            {
                custom_sampler = true;
                wrap_type = wrap;
                interp_type = interp;
            }
        }
    }

    texture_view::texture_view(uint32_t tex_id, uint32_t tex_width, uint32_t tex_height) noexcept
    {
        *this = create(tex_id, tex_width, tex_height);
    }

    bool texture_view::operator==(const texture_view& rhs) const noexcept
    {
        return id == rhs.id && width == rhs.width && height == rhs.height;
    }

    void* texture_view::get() const noexcept
    {
        return reinterpret_cast<void*>(intptr_t(id));
    }

    texture_view texture_view::create(const texture_ptr& texture) noexcept
    {
        texture_view view{};
        if(texture)
        {
            view.blending = texture->get_default_blending_mode();
            view.format = texture->get_pix_type();
            view.width = std::uint32_t(texture->get_rect().w);
            view.height = std::uint32_t(texture->get_rect().h);
            view.id = texture->get_id();
        }

        return view;
    }

    texture_view texture_view::create(uint32_t tex_id, uint32_t tex_width, uint32_t tex_height) noexcept
    {
        texture_view view{};
        view.id = tex_id;
        view.width = tex_width;
        view.height = tex_height;
        view.custom_sampler = true;
        return view;
    }
}
