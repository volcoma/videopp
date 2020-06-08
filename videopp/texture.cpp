
#include "texture.h"

#include "renderer.h"
#include "surface.h"

#include "detail/utils.h"

#include <3rdparty/gli/gli/gli.hpp>
#include <sstream>

namespace gfx
{
    namespace
    {
        constexpr uint64_t max_lod_levels = 2;


        /// Hat pixel format to opengl pixel format converter
        ///     @param pixel_type - hat format to convert
        ///     @return the opengl format
        int32_t get_opengl_pixel_format(const pix_type &pixel_type)
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

        int get_compressed_buffer_size(int width, int height)
        {
            // info for this formula and values is get from :
            // https://www.khronos.org/opengl/wiki/BPTC_Texture_Compression
            // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glCompressedTexImage2D.xhtml
            constexpr int block_width = 4; // pixels
            constexpr int round_up_width = block_width - 1;
            constexpr int block_height = 4; // pixels
            constexpr int round_up_height = block_width - 1;
            constexpr int block_size = 16; // bytes

            return block_size * ((width + round_up_width) / block_width) * ((height + round_up_height) / block_height);
        }

        uint8_t* get_empty_buffer(size_t size)
        {
            static std::vector<uint8_t> empty_data;
            if (empty_data.size() < size_t(size))
            {
                empty_data.resize(size_t(size), 0);
            }

            return empty_data.data();
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
    texture::texture(const renderer &rend, int width, int height, pix_type pixel_type,
                     texture::format_type format_type)
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

        if (texture::format_type::pixmap == format_type_)
        {
            pixmap_ = rend_.context_->create_pixmap(rect_.get_size(), pixel_type_);
        }

        // Generate the OGL texture ID
        int pixel_format = get_opengl_pixel_format(pixel_type);
        gl_call(glGenTextures(1, &texture_));
        rend_.set_texture(texture_, 0);

        // Upload data to VRAM
        if (texture::format_type::target == format_type_)
        {
            //configure LOD
            gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0));
            gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, max_lod_levels));

            gl_call(glTexImage2D(GL_TEXTURE_2D, 0, pixel_format, width, height, 0, static_cast<GLenum> (pixel_format), GL_UNSIGNED_BYTE, nullptr));
        }
        else if (texture::format_type::streaming == format_type_)
        {
            //configure LOD
            gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0));
            gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1));

            gl_call(glTexImage2D(GL_TEXTURE_2D, 0, pixel_format, width, height, 0, static_cast<GLenum> (pixel_format), GL_UNSIGNED_BYTE, nullptr));

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
        else if (texture::format_type::compress == format_type_)
        {
            //configure LOD
            gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0));
            gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, max_lod_levels));

            auto size = get_compressed_buffer_size(width, height);
            auto ptr = get_empty_buffer(size_t(size));
            gl_call(glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, width, height, 0, size, ptr));
        }

        setup_texparameters();

        rend_.reset_texture(0);
    }

    texture::texture(const renderer& rend, const std::tuple<size, pix_type, texture::format_type>& data)
        : texture(rend, std::get<size>(data).w, std::get<size>(data).h,
                  std::get<pix_type>(data), std::get<format_type>(data))
    {
    }

    /// Texture constructor from a surface
    ///     @param rend - renderer interface
    ///     @param surface - the surface to copy pixels from
    texture::texture(const renderer &rend, const surface &surface, bool empty)
        : rend_(rend)
        , pixel_type_(surface.get_type())
        , rect_(0, 0, surface.get_width(), surface.get_height())
    {

        if(!surface.had_alpha_pixels_originally_)
        {
            blending_ = blending_mode::blend_none;
        }

        if (!create_from_surface(surface, empty, 0, 0, 0, surface.get_levels(), surface.get_layers(), surface.get_faces()))
        {
            throw gfx::exception("Cannot create texture from surface.");
        }
    }

    texture::texture(const renderer& rend, const surface& surface, size_t level_id, size_t layer_id, size_t face_id)
        : rend_(rend)
        , pixel_type_(surface.get_type())
    {
        if (surface.get_levels() <= level_id)
        {
            throw gfx::exception("Cannot create texture from surface with fewer lavels than expected.");
        }

        if (surface.get_layers() <= layer_id)
        {
            throw gfx::exception("Cannot create texture from surface with fewer layers than expected.");
        }

        if (surface.get_faces() <= face_id)
        {
            throw gfx::exception("Cannot create texture from surface with fewer faces than expected.");
        }

        rect_ = surface.get_rect(level_id);

        if (!create_from_surface(surface, false, level_id, layer_id, face_id, 1, 1, 1))
        {
            throw gfx::exception("Cannot create texture from surface.");
        }
    }

    bool texture::setup_texparameters()
    {
        // Pick the wrap mode for rendering
        GLint wmode = GL_REPEAT;
        switch(wrap_type_)
        {
        case wrap_type::wrap_mirror:
            wmode = GL_MIRRORED_REPEAT;
            break;
        case wrap_type::wrap_repeat:
            wmode = GL_REPEAT;
            break;
        case wrap_type::wrap_clamp:
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
        case interpolation_type::interpolate_none:
            imode = GL_NEAREST;
            break;
        case interpolation_type::interpolate_linear:
        default:
            imode = GL_LINEAR;
            break;
        }
        gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, imode));
        gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, imode));

        return true;
    }

    texture::~texture()
    {
        rend_.queue_to_delete_texture(pixmap_, fbo_, texture_);
    }

    /// Load from a file
    ///     @param path - the filename
    ///     @return true on success
    bool texture::load_from_file(const std::string &path) noexcept
    {
        try
        {
            surface surf(path);

            if (!create_from_surface(surf, false, 0, 0, 0, surf.get_levels(), surf.get_layers(), surf.get_faces()))
            {
                return false;
            }

            rect_.w = surf.get_width();
            rect_.h = surf.get_height();
        }
        catch (gfx::exception &e)
        {
            log("ERROR: " + std::string(e.what()));
            return false;
        }

        return true;
    }

    /// Copy from a surface
    ///     @param surface - preloaded surface
    ///     @return true on success
    bool texture::create_from_surface(const surface &surface, bool empty, size_t start_level_id, size_t start_layer_id,
                                      size_t start_face_id, size_t levels_count, size_t layers_cout, size_t faces_count) noexcept
    {
        if (!rend_.set_current_context())
        {
            return false;
        }

        gl_call(glGenTextures(1, &texture_));

        auto& gli_surface = *surface.gli_surface_;
        if (!gli::is_compressed(gli_surface.format()))
        {
            format_type_ = format_type::target;
        }
        else
        {
            format_type_ = format_type::compress;
        }

        generated_mipmap_ = levels_count > 1;

        gli::gl gl(gli::gl::PROFILE_GL32);
        auto format = gl.translate(gli_surface.format(), gli_surface.swizzles());
        GLenum target = gl.translate(gli_surface.target());

        gl_call(glBindTexture(target, texture_));
        if( !setup_texparameters() )
        {
            return false;
        }
        if (generated_mipmap_)
        {
            gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
        }

        auto max_levels = std::min(levels_count, max_lod_levels);

        gl_call(glTexParameteri(target, GL_TEXTURE_SWIZZLE_R, format.Swizzles[0]));
        gl_call(glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, format.Swizzles[1]));
        gl_call(glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, format.Swizzles[2]));
        gl_call(glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, format.Swizzles[3]));

        {
            auto face_total = static_cast<GLsizei>(layers_cout * faces_count);
            auto extent = gli_surface.extent(start_level_id);

            switch(gli_surface.target())
            {
            case gli::TARGET_1D:
                gl_call(glTexStorage1D(target, static_cast<GLint>(max_levels),
                                       format.Internal, extent.x));
                break;
            case gli::TARGET_1D_ARRAY:
            case gli::TARGET_2D:
            case gli::TARGET_CUBE:
            {
                gl_call(glTexStorage2D(target, static_cast<GLint>(max_levels),
                                       format.Internal, extent.x,
                                       gli_surface.target() == gli::TARGET_2D ? extent.y : face_total));

// EGL only
//
//                int width = extent.x;
//                int height = gli_surface.target() == gli::TARGET_2D ? extent.y : face_total;
//                for (int i = 0; i < int(max_levels); i++)
//                {
//                    gl_call(glTexImage2D(target, i, format.Internal, width, height, 0,
//                                         format.External, format.Type, nullptr));
//                    width = std::max(1, (width / 2));
//                    height = std::max(1, (height / 2));
//                }
            }
                break;
            case gli::TARGET_2D_ARRAY:
            case gli::TARGET_3D:
            case gli::TARGET_CUBE_ARRAY:
                gl_call(glTexStorage3D(target, static_cast<GLint>(max_levels),
                                       format.Internal, extent.x, extent.y,
                                       gli_surface.target() == gli::TARGET_3D ? extent.z : face_total));
                break;
            default:
                gl_call(glBindTexture(target, 0));
                gl_call(glDeleteTextures(1, &texture_));
                return false;
            }
        }

        if (!empty)
        {
            for(size_t src_layer = start_layer_id, dst_layer = 0;
                src_layer < start_layer_id + layers_cout && dst_layer < layers_cout; ++src_layer, ++dst_layer)
            {
                for(size_t src_face = start_face_id, dst_face = 0;
                    src_face < start_face_id + faces_count && dst_face < faces_count; ++src_face, ++dst_face)
                {
                    for(size_t src_level = start_level_id, dst_level = 0;
                        src_level < start_level_id + max_levels && dst_level < max_levels; ++src_level, ++dst_level)
                    {
                        if(!update({0, 0}, surface, src_level, src_face, src_layer, dst_level, dst_face, dst_layer))
                        {
                            return false;
                        }
                    }
                }
            }
        }

        gl_call(glBindTexture(target, 0));

        return true;
    }

    
    /// Upload new data to VRAM buffer (from surface)
    ///     @param point - offset inside texture pixelspace
    ///     @param surface - surface to copy from
    ///     @return true on success
    bool texture::update(const point &point, const surface &surface)
    {
        const auto& gli_surface = *surface.gli_surface_;
        auto max_levels = std::min(gli_surface.levels(), max_lod_levels);
        for(std::size_t layer = 0; layer < gli_surface.layers(); ++layer)
        {
            for(std::size_t face = 0; face < gli_surface.faces(); ++face)
            {
                for(std::size_t level = 0; level < max_levels; ++level)
                {
                    if(!update(point, surface, level, face, layer, level, face, layer))
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    /// Upload new data to VRAM buffer (from raw data)
    ///     @param rect - rectangle to copy
    ///     @param pix_format - hat pixel format
    ///     @param buffer - data to copy from
    ///     @param level - level of detail (mipmap)
    ///     @return true on success
    bool texture::update(const rect &rect, pix_type pix_format, const uint8_t *buffer, const size_t level)
    {
        const auto format = static_cast<GLenum> (get_opengl_pixel_format(pix_format));
        rend_.set_texture(texture_, 0);

        gl_call(glPixelStorei(GL_UNPACK_ALIGNMENT, bytes_per_pixel(pix_format)));

        gl_call(glTexSubImage2D(GL_TEXTURE_2D, GLint(level), rect.x, rect.y, rect.w, rect.h, format, GL_UNSIGNED_BYTE, buffer));

        gl_call(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));

        rend_.reset_texture(0);
        return true;
    }

    bool texture::update(const point &point, const surface &surface,
                         size_t src_level, size_t src_face, size_t src_layer,
                         size_t dst_level, size_t dst_face, size_t dst_layer)
    {
        const auto& gli_surface = *surface.gli_surface_;
        const auto& src_rect = surface.get_rect(src_level);

        gli::gl gl(gli::gl::PROFILE_GL32);
        auto format = gl.translate(gli_surface.format(), gli_surface.swizzles());
        GLenum target = gl.translate(gli_surface.target());

        gl_call(glBindTexture(target, texture_));

        if(gli::is_compressed(gli_surface.format()))
        {
            gl_call(glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, static_cast<int> (gli_surface.storage().block_size())));
            gl_call(glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, static_cast<int> (gli_surface.storage().block_extent().x)));
            gl_call(glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, static_cast<int> (gli_surface.storage().block_extent().y)));
            gl_call(glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH, static_cast<int> (gli_surface.storage().block_extent().z)));
        }


        auto layer_gl = static_cast<GLsizei>(src_layer);
        math::tvec3<GLsizei> extent(gli_surface.extent(src_level));
        if (gli::is_target_cube(gli_surface.target()))
        {
            target = static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + dst_face);
        }

        switch(gli_surface.target())
        {
            case gli::TARGET_1D:
                if(gli::is_compressed(gli_surface.format()))
                {
                    gl_call(glCompressedTexSubImage1D(target, static_cast<GLint>(dst_level), point.x, src_rect.w,
                                                      format.Internal, static_cast<GLsizei>(gli_surface.size(src_level)),
                                                      gli_surface.data(src_layer, src_face, src_level)));
                }
                else
                {
                    gl_call(glTexSubImage1D(target, static_cast<GLint>(dst_level), point.x, src_rect.w,
                                            format.External, format.Type,
                                            gli_surface.data(src_layer, src_face, src_level)));
                }
                break;
            case gli::TARGET_1D_ARRAY:
            case gli::TARGET_2D:
            case gli::TARGET_CUBE:
                if(gli::is_compressed(gli_surface.format()))
                {
                    gl_call(glCompressedTexSubImage2D(target, static_cast<GLint>(dst_level), point.x, point.y, src_rect.w,
                                                      gli_surface.target() == gli::TARGET_1D_ARRAY ? layer_gl : src_rect.h,
                                                      format.Internal, static_cast<GLsizei>(gli_surface.size(src_level)),
                                                      gli_surface.data(src_layer, src_face, src_level)));
                }
                else
                {
                    gl_call(glTexSubImage2D(target, static_cast<GLint>(dst_level), point.x, point.y, src_rect.w,
                                            gli_surface.target() == gli::TARGET_1D_ARRAY ? layer_gl : src_rect.h,
                                            format.External, format.Type,
                                            gli_surface.data(src_layer, src_face, src_level)));

                }
                break;
            case gli::TARGET_2D_ARRAY:
            case gli::TARGET_3D:
            case gli::TARGET_CUBE_ARRAY:
                if(gli::is_compressed(gli_surface.format()))
                {
                    gl_call(glCompressedTexSubImage3D(target, static_cast<GLint>(dst_level), point.x, point.y, 0,
                                                      src_rect.w, src_rect.h,
                                                      gli_surface.target() == gli::TARGET_3D ? extent.z : layer_gl,
                                                      format.Internal, static_cast<GLsizei>(gli_surface.size(src_level)),
                                                      gli_surface.data(src_layer, src_face, src_level)));
                }
                else
                {
                    gl_call(glTexSubImage3D(target, static_cast<GLint>(dst_level), point.x, point.y, 0,
                                            src_rect.w, src_rect.h,
                                            gli_surface.target() == gli::TARGET_3D ? extent.z : layer_gl,
                                            format.External, format.Type,
                                            gli_surface.data(src_layer, src_face, src_level)));
                }
                break;
            default:
                gl_call(glBindTexture(target, 0));
                return false;
        }


        gl_call(glBindTexture(target, 0));

        if(gli::is_compressed(gli_surface.format()))
        {
            gl_call(glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, 0));
            gl_call(glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 0));
            gl_call(glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 0));
            gl_call(glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH, 0));
        }

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
        gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0));
        gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, max_lod_levels));
//        gl_call(glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST));
        gl_call(glGenerateMipmap(GL_TEXTURE_2D));
        gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
        gl_call(glBindTexture(GL_TEXTURE_2D, 0));

        generated_mipmap_ = true;
        return generated_mipmap_;
    }

    void texture::set_default_blending_mode(blending_mode mode)
    {
        blending_ = mode;
    }

    const void* texture::get_pixmap_native_handle() const
    {
        return rend_.context_->get_native_handle(pixmap_);
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
            view.pixmap = texture->get_pixmap();
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
