#include "renderer.h"
#include "font.h"
#include "ttf_font.h"
#include "texture.h"
#include "shaders.h"
#include "detail/utils.h"

#ifdef WGL_CONTEXT
#include "detail/wgl/context_wgl.h"
#elif GLX_CONTEXT
#include "detail/glx/context_glx.h"
#elif EGL_CONTEXT
#include "detail/egl/context_egl.h"
#else

#endif

namespace video_ctrl
{

inline GLenum to_gl_primitive(primitive_type type)
{
    switch(type)
    {
        case primitive_type::lines:
            return GL_LINES;
        case primitive_type::lines_loop:
            return GL_LINE_LOOP;
        case primitive_type::triangle_fan:
            return GL_TRIANGLE_FAN;
        case primitive_type::triangle_strip:
            return GL_TRIANGLE_STRIP;
        default:
            return GL_TRIANGLES;
    }
}

/// Construct the renderer and initialize default rendering states
///	@param win - the window handle
///	@param vsync - true to enable vsync
renderer::renderer(os::window& win, bool vsync)
    : win_(win)
#ifdef WGL_CONTEXT
    , context_(std::make_unique<context_wgl>(win.get_native_handle()))
#elif GLX_CONTEXT
    , context_(std::make_unique<context_glx>(win.get_native_handle(), win.get_native_display()))
#elif EGL_CONTEXT
    , context_(std::make_unique<context_egl>(win.get_native_handle(), win.get_native_display()))
#endif
{
    context_->set_vsync(vsync);

    if(!gladLoadGL())
    {
        throw video_ctrl::exception("Cannot load glad.");
    }

    //rect_.x = win_.get_position().x;
    //rect_.y = win_.get_position().y;
    rect_.w = win_.get_size().w;
    rect_.h = win_.get_size().h;

    gl_call(glDisable(GL_DEPTH_TEST));
    gl_call(glDepthMask(GL_FALSE));

    // Default blending mode
    set_blending_mode(blending_mode::blend_normal);

    // Default texture interpolation methods
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    // Default texture wrapping methods
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));

    // Default vertex buffer budget. This is not static, so no worries
    for(auto& vbo : stream_vbos_)
    {
        vbo.create();
        vbo.bind();
        vbo.reserve(nullptr, sizeof(vertex_2d) * 1024, true);
        vbo.unbind();
    }

    // Default index buffer budget. This is not static, so no worries
    for(auto& ibo : stream_ibos_)
    {
        ibo.create();
        ibo.bind();
        ibo.reserve(nullptr, sizeof(draw_list::index_t) * 1024, true);
        ibo.unbind();
    }

    reset_transform();
    set_model_view(0, rect_);

    {
        auto& program = simple_program();
        if(!program.shader)
        {
            auto shader = create_shader(fs_simple, vs_simple);
            embedded_shaders_.emplace_back(shader);
            program.shader = shader.get();
            auto& layout = program.shader->get_layout();
            layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition", sizeof(vertex_2d));
            layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", sizeof(vertex_2d), true);
        }
    }

    {
        auto& program = multi_channel_texture_program();
        if(!program.shader)
        {
            auto shader = create_shader(fs_multi_channel_dither, vs_simple);
            embedded_shaders_.emplace_back(shader);
            program.shader = shader.get();
            auto& layout = program.shader->get_layout();
            layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition", sizeof(vertex_2d));
            layout.add<float>(2, offsetof(vertex_2d, uv), "aTexCoord", sizeof(vertex_2d));
            layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", sizeof(vertex_2d), true);
        }
    }

    {
        auto& program = single_channel_texture_program();
        if(!program.shader)
        {
            auto shader = create_shader(fs_single_channel, vs_simple);
            embedded_shaders_.emplace_back(shader);
            program.shader = shader.get();
            auto& layout = program.shader->get_layout();
            layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition", sizeof(vertex_2d));
            layout.add<float>(2, offsetof(vertex_2d, uv), "aTexCoord", sizeof(vertex_2d));
            layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", sizeof(vertex_2d), true);
        }
    }

    {
        auto& program = distance_field_font_program();
        if(!program.shader)
        {
            auto shader = create_shader(fs_distance_field, vs_simple);
            embedded_shaders_.emplace_back(shader);
            program.shader = shader.get();
            auto& layout = program.shader->get_layout();
            layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition", sizeof(vertex_2d));
            layout.add<float>(2, offsetof(vertex_2d, uv), "aTexCoord", sizeof(vertex_2d));
            layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", sizeof(vertex_2d), true);
        }
    }


    {
        auto& program = blur_program();
        if(!program.shader)
        {
            auto shader = create_shader(fs_blur, vs_simple);
            embedded_shaders_.emplace_back(shader);
            program.shader = shader.get();
            auto& layout = program.shader->get_layout();
            layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition", sizeof(vertex_2d));
            layout.add<float>(2, offsetof(vertex_2d, uv), "aTexCoord", sizeof(vertex_2d));
            layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", sizeof(vertex_2d), true);
        }
    }

    {
        auto& program = fxaa_program();
        if(!program.shader)
        {
            auto shader = create_shader(fs_fxaa, vs_simple);
            embedded_shaders_.emplace_back(shader);
            program.shader = shader.get();
            auto& layout = program.shader->get_layout();
            layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition", sizeof(vertex_2d));
            layout.add<float>(2, offsetof(vertex_2d, uv), "aTexCoord", sizeof(vertex_2d));
            layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", sizeof(vertex_2d), true);
        }
    }
    if(!default_font())
    {
        default_font() = create_font(create_default_font(13, 2));
        embedded_fonts_.emplace_back(default_font());
    }
    clear(color::black());
}

renderer::~renderer()
{
    embedded_fonts_.clear();
    embedded_shaders_.clear();

    auto clear_embedded = [](auto& shared)
    {
        if(shared.use_count() == 1)
        {
            shared.reset();
        }
    };

    clear_embedded(default_font());
}

/// Bind FBO, set projection, switch to modelview
///	@param fbo - framebuffer object, use 0 for the backbuffer
///	@param rect - the dirty area to draw into
void renderer::set_model_view(const uint32_t fbo, const rect& rect) const noexcept
{
    // Bind the FBO or backbuffer if 0
    gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    // Set the viewport
    gl_call(glViewport(0, 0, rect.w, rect.h));

    // Set the projection
    if(fbo == 0)
    {
        // If we have 0 it is the back buffer
        current_ortho_ = math::ortho<float>(0, rect.w, rect.h, 0, 0, FARTHEST_Z);
    }
    else
    {
        // If we have > 0 the fbo must be flipped
        current_ortho_ = math::ortho<float>(0, rect.w, 0, rect.h, 0, FARTHEST_Z);
    }

    // Switch to modelview matrix and setup identity
    reset_transform();
}

void renderer::clear_fbo(uint32_t fbo_id, const color &color) const noexcept
{
    set_current_context();
    gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo_id));
    gl_call(glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f));
    gl_call(glClear(GL_COLOR_BUFFER_BIT));

    set_old_framebuffer();
}

/// Bind this renderer & window
///	@return true on success
bool renderer::set_current_context() const noexcept
{
    return context_->make_current();
}

/// 
void renderer::set_old_framebuffer() const noexcept
{
    if (fbo_stack_.empty())
    {
        gl_call(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        return;
    }

    const auto& top_texture = fbo_stack_.top();
    gl_call(glBindFramebuffer(GL_FRAMEBUFFER, top_texture->get_FBO()));
}

/// Resize the dirty rectangle
/// @param new_width - the new width
/// @param new_height - the new height
void renderer::resize(uint32_t new_width, uint32_t new_height) noexcept
{
    rect_.w = new_width;
    rect_.h = new_height;
}

/// Create a texture from a surface
///	@param surface - surface to extract data from
///	@param allocatorName - allocator name
///	@param fileName - file name
///	@return a shared pointer to the produced texture
texture_ptr renderer::create_texture(const surface& surface) const noexcept
{
    if(!set_current_context())
    {
        return {};
    }

    try
    {
        auto tex = new texture(*this, surface);
        return texture_ptr(tex);
    }
    catch(const std::exception& e)
    {
        log(std::string("ERROR: Cannot create texture from surface. Reason ") + e.what());
    }

    return texture_ptr(nullptr);
}

/// Create a texture from a filename
///	@param file_name - file name
///	@param allocatorName - allocator name
///	@return a shared pointer to the produced texture
texture_ptr renderer::create_texture(const std::string& file_name) const noexcept
{
    if(!set_current_context())
    {
        return {};
    }

    try
    {
        auto tex = new texture(*this);
        texture_ptr texture(tex);

        if(!texture->load_from_file(file_name))
        {
            return texture_ptr();
        }

        return texture;
    }
    catch(const std::exception& e)
    {
        log(std::string("ERROR: Cannot create texture from file. Reason ") + e.what());
    }

    return texture_ptr();
}

/// Create a blank texture
///	@param w, h - size of texture
///	@param pixel_type - color type of pixel
///	@param format_type - texture puprose
///	@param allocatorName - allocator name
///	@return a shared pointer to the produced texture
texture_ptr renderer::create_texture(int w, int h, pix_type pixel_type, texture::format_type format_type) const noexcept
{
    if(!set_current_context())
    {
        return {};
    }

    try
    {
        std::string name = format_type == texture::format_type::streaming ? "streaming" : "target";
        auto tex = new texture(*this, w, h, pixel_type, format_type);
        return texture_ptr(tex);
    }
    catch(const std::exception& e)
    {
        log(std::string("ERROR: Cannot create blank texture. Reason ") + e.what());
    }

    return texture_ptr();
}

/// Create a shader
///	@param fragment_code - fragment shader written in GLSL
///	@param vertex_code - vertex shader code written in GLSL
///	@return a shared pointer to the shader
shader_ptr renderer::create_shader(const char* fragment_code, const char* vertex_code) const noexcept
{
    if(!set_current_context())
    {
        return {};
    }

    try
    {
        auto* shad = new shader(*this, fragment_code, vertex_code);
        return shader_ptr(shad);
    }
    catch(const std::exception& e)
    {
        log(std::string("ERROR: Cannot create shader. Reason ") + e.what());
    }

    return {};
}

/// Create a font
///	@param info - description of font
///	@return a shared pointer to the font
font_ptr renderer::create_font(font_info&& info) const noexcept
{
    if(!set_current_context())
    {
        return {};
    }

    try
    {
        auto r = std::make_shared<font>();
        font_info& slice = *r;
        slice = std::move(info);

        r->texture = texture_ptr(new texture(*this, *r->surface));
        r->surface.reset();
        return r;
    }
    catch(const std::exception& e)
    {
        log(std::string("ERROR: Cannot create font. Reason ") + e.what());
    }


    return {};
}

/// Set the blending mode for drawing
///	@param mode - blending mode to set
///	@return true on success
bool renderer::set_blending_mode(blending_mode mode) const noexcept
{
    if(!set_current_context())
    {
        return false;
    }

    switch(mode)
    {
        case blending_mode::blend_none:
            gl_call(glDisable(GL_BLEND));
            break;
        case blending_mode::blend_normal:
            gl_call(glEnable(GL_BLEND));
            gl_call(glBlendEquation(GL_FUNC_ADD));
            gl_call(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case blending_mode::blend_add:
            gl_call(glEnable(GL_BLEND));
            gl_call(glBlendEquation(GL_FUNC_ADD));
            gl_call(glBlendFunc(GL_SRC_ALPHA, GL_ONE));
            break;
        case blending_mode::blend_lighten:
            gl_call(glEnable(GL_BLEND));
            gl_call(glBlendEquation(GL_MAX));
            gl_call(glBlendFunc(GL_ONE, GL_ONE));
            break;
        case blending_mode::pre_multiplication:
            gl_call(glEnable(GL_BLEND));
            gl_call(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE));
            break;
        case blending_mode::unmultiplied_alpha:
            gl_call(glEnable(GL_BLEND));
            gl_call(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case blending_mode::blend_screen:
        default:
            // TODO
            gl_call(glEnable(GL_BLEND));
            break;
    }

    return true;
}

/// Lowest level texture binding call, for both fixed pipeline and shaders, and provide optional wrapping and interpolation modes
///     @param texture - the texture view
///     @param id - texture unit id (for the shader)
///     @param wrap_type - wrapping type
///     @param interp_type - interpolation type
bool renderer::bind_texture(texture_view texture, uint32_t id, texture::wrap_type wtype, texture::interpolation_type itype) const noexcept
{
    // Activate texture for shader
    gl_call(glActiveTexture(GL_TEXTURE0 + id));

    // Bind texture to the pipeline, and the currently active texture
    gl_call(glBindTexture(GL_TEXTURE_2D, texture.id));

    // Pick the wrap mode for rendering
    GLint wmode = GL_REPEAT;
    switch(wtype)
    {
        case texture::wrap_type::clamp:
            wmode = GL_CLAMP_TO_EDGE;
            break;
        case texture::wrap_type::mirror:
            wmode = GL_MIRRORED_REPEAT;
            break;
        default:
            wmode = GL_REPEAT;
            break;
    }
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wmode));
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wmode));

    // Pick the interpolation mode for rendering
    GLint imode = GL_LINEAR;
    switch(itype)
    {
        case texture::interpolation_type::nearest:
            imode = GL_NEAREST;
            break;
        case texture::interpolation_type::linear:
            imode = GL_LINEAR;
            break;
    }
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, imode));
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, imode));

    // Check errors and return
    return true;
}

/// Reset a texture slot
///     @param id - the texture to reset
void renderer::unbind_texture(uint32_t id) const noexcept
{
    gl_call(glActiveTexture(GL_TEXTURE0 + id));
    gl_call(glBindTexture(GL_TEXTURE_2D, 0));
}

texture_ptr renderer::blur(const texture_ptr& texture, uint32_t passes)
{
    passes = std::max<uint32_t>(1, passes);

    texture_ptr input = texture;
    auto radius = passes / 2.0f;

    for(uint32_t i = 0; i < passes; ++i)
    {
        if(i % 2 == 0)
        {
            radius = ((passes - i)/ 2.0f);
        }
        auto input_size = math::vec2(input->get_rect().w, input->get_rect().h);
        auto fbo = create_texture(int(input_size.x), int(input_size.y), input->get_pix_type(), texture::format_type::streaming);
        auto direction = i % 2 == 0 ? math::vec2{radius, 0.0f} : math::vec2{0.0f, radius};

        if (!push_fbo(fbo))
        {
            return {};
        }

        draw_list list;

        video_ctrl::program_setup setup;
        setup.program = blur_program();
        setup.begin = [=](const video_ctrl::gpu_context& ctx)
        {
            ctx.program.shader->set_uniform("uTexture", input);
            ctx.program.shader->set_uniform("uTextureSize", input_size);
            ctx.program.shader->set_uniform("uDirection", direction);
        };

        list.add_image(input, input->get_rect(), fbo->get_rect(), color::white(), flip_format::none, setup);

        draw_cmd_list(list);

        if (!pop_fbo())
        {
            return {};
        }

        input = fbo;
    }

    return input;
}

/// Push and load a modelview matrix
///     @param transform - the transformation to push
///     @return true on success
bool renderer::push_transform(const math::transformf& transform) const noexcept
{
    transforms_.push(transform);

    return true;
}

/// Restore a previously set matrix
///     @return true on success
bool renderer::pop_transform() const noexcept
{
    transforms_.pop();

    if (transforms_.empty())
    {
        reset_transform();
    }
    return true;
}

/// Reset the modelview transform to identity
///     @return true on success
bool renderer::reset_transform() const noexcept
{
    decltype (transforms_) new_tranform;
    new_tranform.push(math::transformf::identity());
    transforms_.swap(new_tranform);

    return true;
}

/// Set scissor test rectangle
///	@param rect - rectangle to clip
///	@return true on success
bool renderer::push_clip(const rect& rect) const noexcept
{
    if (!set_current_context())
    {
        return false;
    }

    gl_call(glEnable(GL_SCISSOR_TEST));

    if (fbo_stack_.empty())
    {
        // If we have no fbo_target_ it is a back buffer
        gl_call(glScissor(rect.x, rect_.h - rect.y - rect.h, rect.w, rect.h));
    }
    else
    {
        gl_call(glScissor(rect.x, rect.y, rect.w, rect.h));
    }

    return true;
}

bool renderer::pop_clip() const noexcept
{
    if (!set_current_context())
    {
        return false;
    }

    gl_call(glDisable(GL_SCISSOR_TEST));

    return true;
}

/// Set FBO target for drawing
///	@param texture - texture to draw to
///	@return true on success
bool renderer::push_fbo(const texture_ptr& texture)
{
    if (texture->format_type_ != texture::format_type::streaming)
    {
        return false;
    }

    if (!set_current_context())
    {
        return false;
    }

    fbo_stack_.push(texture);

    auto& top_texture = fbo_stack_.top();
    set_model_view(top_texture->get_FBO(), top_texture->get_rect());
    return true;
}

/// Set old FBO target for drawing. if empty set draw to back render buffer
///	@return true on success
bool renderer::pop_fbo()
{
    if (fbo_stack_.empty())
    {
        return false;
    }

    if (!set_current_context())
    {
        return false;
    }

    fbo_stack_.pop();

    if (fbo_stack_.empty())
    {
        set_model_view(0, rect_);
        return true;
    }

    auto& top_texture = fbo_stack_.top();
    set_model_view(top_texture->get_FBO(), top_texture->get_rect());
    return true;
}

/// Remove all FBO targets from stack and set draw to back render buffer
///	@return true on success
bool renderer::reset_fbo()
{
    while (!fbo_stack_.empty())
    {
        fbo_stack_.pop();
    }

    set_model_view(0, rect_);
    return true;
}

/// Swap buffers
void renderer::present() noexcept
{
    auto sz = win_.get_size();
    resize(sz.w, sz.h);

    draw_list list{};
    list.add_line({0, 0}, {1, 1}, {255, 255, 255, 0});
    draw_cmd_list(list);

    context_->swap_buffers();

    set_current_context();
    set_model_view(0, rect_);
}

/// Clear the currently bound buffer
///	@param color - color to clear with
void renderer::clear(const color& color) const noexcept
{
    set_current_context();

    gl_call(glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f));
    gl_call(glClear(GL_COLOR_BUFFER_BIT));
}

constexpr uint16_t get_index_type()
{
    if(sizeof(draw_list::index_t) == sizeof(uint32_t))
    {
        return GL_UNSIGNED_INT;
    }

    return GL_UNSIGNED_SHORT;
}
/// Render a draw list
///	@param list - list to draw
///	@return true on success
bool renderer::draw_cmd_list(const draw_list& list) const noexcept
{
    if (!set_current_context())
    {
        return false;
    }

    if (list.vertices.empty())
    {
        return false;
    }

    log(list.to_string());
    list.validate_stacks();

    const auto vtx_stride = sizeof(decltype(list.vertices)::value_type);
    const auto vertices_mem_size = list.vertices.size() * vtx_stride;
    const auto idx_stride = sizeof(decltype(list.indices)::value_type);
    const auto indices_mem_size = list.indices.size() * idx_stride;

    // We are using several stream buffers to avoid syncs caused by
    // uploading new data while the old one is still processing
    auto& vbo = stream_vbos_[current_vbo_idx_];
    auto& ibo = stream_ibos_[current_ibo_idx_];

    current_vbo_idx_ = (current_vbo_idx_ + 1) % stream_vbos_.size();
    current_ibo_idx_ = (current_ibo_idx_ + 1) % stream_ibos_.size();

    // Bind the vertex buffer
    vbo.bind();

    // Upload vertices to VRAM
    if (!vbo.update(list.vertices.data(), 0, vertices_mem_size))
    {
        // We're out of vertex budget. Allocate a new vertex buffer
        vbo.reserve(list.vertices.data(), vertices_mem_size, true);
    }

    // Bind the index buffer
    ibo.bind();

    // Upload indices to VRAM
    if (!ibo.update(list.indices.data(), 0, indices_mem_size))
    {
        // We're out of index budget. Allocate a new index buffer
        ibo.reserve(list.indices.data(), indices_mem_size, true);
    }


    {
        blending_mode last_blend{blending_mode::blend_none};
        set_blending_mode(last_blend);

        shader* last_shader{};

        // Draw commands
        for (const auto& cmd : list.commands)
        {
            {
                if(cmd.setup.program.shader && cmd.setup.program.shader != last_shader)
                {
                    cmd.setup.program.shader->enable();

                    last_shader = cmd.setup.program.shader;
                }

                if (cmd.clip_rect)
                {
                    push_clip(cmd.clip_rect);
                }

                if (cmd.blend != last_blend)
                {
                    set_blending_mode(cmd.blend);
                    last_blend = cmd.blend;
                }

                if(cmd.setup.begin)
                {
                    cmd.setup.begin(gpu_context{*this, cmd.setup.program});
                }

                if(cmd.setup.program.shader && cmd.setup.program.shader->has_uniform("uProjection"))
                {
                    const auto& projection = current_ortho_ * transforms_.top();
                    cmd.setup.program.shader->set_uniform("uProjection", projection);
                }

            }

            switch(cmd.dr_type)
            {
                case draw_type::elements:
                {
                    gl_call(glDrawElements(to_gl_primitive(cmd.type), GLsizei(cmd.indices_count), get_index_type(),
                                           reinterpret_cast<const GLvoid*>(uintptr_t(cmd.indices_offset * idx_stride))));
                }
                break;

                case draw_type::array:
                {
                    gl_call(glDrawArrays(to_gl_primitive(cmd.type), GLint(cmd.vertices_offset), GLsizei(cmd.vertices_count)));
                }
                break;
            }


            {
                if (cmd.setup.end)
                {
                    cmd.setup.end(gpu_context{*this, cmd.setup.program});
                }

                if (cmd.clip_rect)
                {
                    pop_clip();
                }
            }
        }


        if(last_shader)
        {
            last_shader->disable();
        }

        set_blending_mode(blending_mode::blend_none);
    }

    vbo.unbind();
    ibo.unbind();

    return true;
}

/// Enable vertical synchronization to avoid tearing
///     @return true on success
bool renderer::enable_vsync() noexcept
{
//    if (!vsync_enabled_)
//    {
//        if (0 != SDL_GL_SetSwapInterval(-1))
//        {
//            if (0 != SDL_GL_SetSwapInterval(1))
//            {
//                // TODO Remove it when all move to new version of linux
//                // throw renderer::inner_exception("Cannot set SwapInterval.");
//                log("ERROR: Cannot set vsync swap interval!");
//                return false;
//            }
//        }
//        vsync_enabled_ = true;
//    }

    return context_->set_vsync(true);

}

/// Disable vertical synchronization
///     @return true on success
bool renderer::disable_vsync() noexcept
{
    return context_->set_vsync(false);
}

/// Get the dirty rectangle we're drawing into
///     @return the currently used rect
const rect& renderer::get_rect() const
{
    if (fbo_stack_.empty())
    {
        return rect_;
    }

    const auto& top_texture = fbo_stack_.top();
    return top_texture->get_rect();
}
}
