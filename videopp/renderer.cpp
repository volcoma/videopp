#include "renderer.h"
#include "font.h"
#include "ttf_font.h"
#include "texture.h"
#include "shaders.h"
#include "utils.h"

#ifdef WGL_CONTEXT
#include "detail/context_wgl.h"
#elif GLX_CONTEXT
#include "detail/context_glx.h"
#elif EGL_CONTEXT
#include "detail/context_egl.h"
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
#elif EGL_CONTEXT
    , context_(std::make_unique<context_egl>(win.get_native_handle(), win.get_native_display()))
#elif GLX_CONTEXT
    , context_(std::make_unique<context_glx>(win.get_native_handle(), win.get_native_display()))
#endif
{
    context_->set_vsync(vsync);

    if(!gladLoadGL())
    {
        throw video_ctrl::exception("Cannot load glad.");
    }

    rect_.x = win_.get_position().x;
    rect_.y = win_.get_position().y;
    rect_.w = win_.get_size().w;
    rect_.h = win_.get_size().h;

    // Enable Texture Mapping (NEW)
    //gl_call(glEnable(GL_TEXTURE_2D));

    // Depth calculation
    //gl_call(glClearDepth(1.0));
    //gl_call(glDepthFunc(GL_LEQUAL));
    gl_call(glDisable(GL_DEPTH_TEST));
    gl_call(glDepthMask(GL_FALSE));

    // Default blending mode
    //gl_call(glAlphaFunc(GL_GREATER, 0.1f));
    set_blending_mode(blending_mode::blend_normal);

    // Default texture interpolation methods
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    // Default texture wrapping methods
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));

    // Default vertex buffer budget. This is not static, so no worries
    master_vbo_.create();
    master_vbo_.reserve(nullptr, sizeof(vertex_2d) * 1024, true);

    // Default index buffer budget. This is not static, so no worries
    master_ibo_.create();
    master_ibo_.reserve(nullptr, sizeof(draw_list::index_t) * 1024, true);

    reset_transform();
    set_model_view(0, rect_);

    {
        auto& program = simple_program();
        if(!program.shader)
        {
            program.shader = create_shader(fs_simple, vs_simple);;
            program.layout.set_program_id(program.shader->get_program_id());
            program.layout.set_stride(sizeof(vertex_2d));
            program.layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition");
            program.layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", true);
            embedded_shaders_.emplace_back(program.shader);
        }
    }

    {
        auto& program = multi_channel_texture_program();
        if(!program.shader)
        {
            program.shader = create_shader(fs_multi_channel, vs_simple);
            program.layout.set_program_id(program.shader->get_program_id());
            program.layout.set_stride(sizeof(vertex_2d));
            program.layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition");
            program.layout.add<float>(2, offsetof(vertex_2d, uv), "aTexCoord");
            program.layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", true);
            embedded_shaders_.emplace_back(program.shader);
        }
    }

    {
        auto& program = single_channel_texture_program();
        if(!program.shader)
        {
            program.shader = create_shader(fs_single_channel, vs_simple);
            program.layout.set_program_id(program.shader->get_program_id());
            program.layout.set_stride(sizeof(vertex_2d));
            program.layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition");
            program.layout.add<float>(2, offsetof(vertex_2d, uv), "aTexCoord");
            program.layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", true);
            embedded_shaders_.emplace_back(program.shader);
        }
    }

    {
        auto& program = distance_field_font_program();
        if(!program.shader)
        {
            program.shader = create_shader(fs_distance_field, vs_simple);
            program.layout.set_program_id(program.shader->get_program_id());
            program.layout.set_stride(sizeof(vertex_2d));
            program.layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition");
            program.layout.add<float>(2, offsetof(vertex_2d, uv), "aTexCoord");
            program.layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", true);
            embedded_shaders_.emplace_back(program.shader);
        }
    }


    {
        auto& program = blur_program();
        if(!program.shader)
        {
            program.shader = create_shader(fs_blur, vs_simple);
            program.layout.set_program_id(program.shader->get_program_id());
            program.layout.set_stride(sizeof(vertex_2d));
            program.layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition");
            program.layout.add<float>(2, offsetof(vertex_2d, uv), "aTexCoord");
            program.layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", true);
            embedded_shaders_.emplace_back(program.shader);
        }
    }

    {
        auto& program = fxaa_program();
        if(!program.shader)
        {
            program.shader = create_shader(fs_fxaa, vs_simple);
            program.layout.set_program_id(program.shader->get_program_id());
            program.layout.set_stride(sizeof(vertex_2d));
            program.layout.add<float>(2, offsetof(vertex_2d, pos), "aPosition");
            program.layout.add<float>(2, offsetof(vertex_2d, uv), "aTexCoord");
            program.layout.add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", true);
            embedded_shaders_.emplace_back(program.shader);
        }
    }
    if(!default_font())
    {
        default_font() = create_font(create_default_font());
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

    clear_embedded(simple_program().shader);
    clear_embedded(multi_channel_texture_program().shader);
    clear_embedded(single_channel_texture_program().shader);
    clear_embedded(distance_field_font_program().shader);
    clear_embedded(blur_program().shader);
    clear_embedded(fxaa_program().shader);
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
    //gl_call(glMatrixMode(GL_PROJECTION));
    //gl_call(glLoadIdentity());
    
    // Set the projection
    if(fbo == 0)
    {
        // If we have 0 it is the back buffer
        //gl_call(glOrtho(0, rect.w, rect.h, 0, 0, FARTHEST_Z));
        current_ortho_ = math::ortho<float>(0, rect.w, rect.h, 0, 0, FARTHEST_Z);
    }
    else
    {
        // If we have > 0 the fbo must be flipped
        //gl_call(glOrtho(0, rect.w, 0, rect.h, 0, FARTHEST_Z));
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
void renderer::resize(int new_width, int new_height) noexcept
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
    catch(video_ctrl::exception& e)
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
    catch(video_ctrl::exception& e)
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
    catch(video_ctrl::exception& e)
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
    catch(video_ctrl::exception& e)
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
    catch(video_ctrl::exception& e)
    {
        log(std::string("ERROR: Cannot create font. Reason ") + e.what());
    }


    return {};
}

/// Set the blending mode for drawing
///	@param mode - blending mode to set
///	@return true on success
bool renderer::set_blending_mode(const blending_mode mode) const noexcept
{
    if(!set_current_context())
    {
        return false;
    }

    switch(mode)
    {
        case video_ctrl::blending_mode::blend_none:
            gl_call(glDisable(GL_BLEND));
            break;
        case video_ctrl::blending_mode::blend_normal:
            gl_call(glEnable(GL_BLEND));
            gl_call(glBlendEquation(GL_FUNC_ADD));
            gl_call(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case video_ctrl::blending_mode::blend_add:
            gl_call(glEnable(GL_BLEND));
            gl_call(glBlendEquation(GL_FUNC_ADD));
            gl_call(glBlendFunc(GL_SRC_ALPHA, GL_ONE));
            break;
        case video_ctrl::blending_mode::blend_lighten:
            gl_call(glEnable(GL_BLEND));
            gl_call(glBlendEquation(GL_MAX));
            gl_call(glBlendFunc(GL_ONE, GL_ONE));
            break;
        case video_ctrl::blending_mode::pre_multiplication:
            gl_call(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE));
            break;
        case video_ctrl::blending_mode::unmultiplied_alpha:
            gl_call(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case video_ctrl::blending_mode::blend_screen:
        default:
            // TODO
            gl_call(glEnable(GL_BLEND));
            break;
    }

    return true;
}

/// Lowest level texture binding call, for both fixed pipeline and shaders, and provide optional wrapping and interpolation modes
///     @param texture - the texture id (internal GL id)
///     @param id - texture unit id (for the shader)
///     @param wrap_type - wrapping type
///     @param interp_type - interpolation type
bool renderer::set_texture(const uint32_t texture, uint32_t id, texture::wrap_type wtype, texture::interpolation_type itype) const noexcept 
{
    // Check if texture is valid
    if (!texture)
    {
        return false;
    }

    // Activate texture for shader
    gl_call(glActiveTexture(GL_TEXTURE0 + id));
    
    // Bind texture to the pipeline, and the currently active texture
    gl_call(glBindTexture(GL_TEXTURE_2D, texture));
    
    // Pick the wrap mode for rendering
    GLint wmode = GL_REPEAT;
    switch(wtype)
    {
        case texture::wrap_type::wrap_clamp: 
            wmode = GL_CLAMP_TO_EDGE;
            break;
        case texture::wrap_type::wrap_mirror: 
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
        case texture::interpolation_type::interpolate_none:
            imode = GL_NEAREST;
            break;
        case texture::interpolation_type::interpolate_linear:
            imode = GL_LINEAR;
            break;
    }
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, imode));
    gl_call(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, imode));
    
    // Check errors and return
    return true;
}

/// Bind a texture view, for both fixed pipeline and shaders, and provide optional wrapping and interpolation modes
///     @param texture - the texture view
///     @param id - texture unit id (for the shader)
///     @param wrap_type - wrapping type
///     @param interp_type - interpolation type
bool renderer::set_texture(texture_view texture, uint32_t id, texture::wrap_type wtype, texture::interpolation_type itype) const noexcept
{
    // Check if texture is valid
    if (!texture.is_valid())
    {
        return false;
    }
    
    return set_texture(texture.id, id, wtype, itype);
}

/// Bind a texture, for both fixed pipeline and shaders
/// and provide optional wrapping and interpolation modes
///     @param texture - the texture
///     @param id - texture unit id (for the shader)
///     @param wrap_type - wrapping type
///     @param interp_type - interpolation type
bool renderer::set_texture(const texture_ptr& texture, uint32_t id, texture::wrap_type wtype, texture::interpolation_type itype) const noexcept 
{
    const video_ctrl::texture_view view(texture);
    return set_texture(view, id, wtype, itype);
}

/// Reset a texture slot
///     @param id - the texture to reset
void renderer::reset_texture(uint32_t id) const noexcept
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

        list.add_image(input, input->get_rect(), fbo->get_rect(), {}, color::white(), setup);

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

/// Enable a scissor test (including viewport and ortho)
///	@param rect - rectangle to clip
///	@return true on success
bool renderer::set_clip_rect(const rect& rect) const noexcept
{
    if(!set_current_context())
    {
        return false;
    }

    gl_call(glMatrixMode(GL_PROJECTION));
    gl_call(glLoadIdentity());
    
    if(fbo_stack_.empty())
    {
        // If we have no fbo_target_ it is back buffer
        gl_call(glViewport(rect.x, rect_.h - rect.y - rect.h, rect.w, rect.h));
        gl_call(glOrtho(rect.x, rect.x + rect.w, rect.y + rect.h, rect.y, 0, FARTHEST_Z));
        current_ortho_ = math::ortho<float>(rect.x, rect.x + rect.w, rect.y + rect.h, rect.y, 0, FARTHEST_Z);
    }
    else
    {
        // if we have it is fbo must be flipped
        gl_call(glViewport(rect.x, rect.y, rect.w, rect.h));
        gl_call(glOrtho(rect.x, rect.x + rect.w, rect.y, rect.y + rect.h, 0, FARTHEST_Z));
        current_ortho_ = math::ortho<float>(rect.x, rect.x + rect.w, rect.y, rect.y + rect.h, 0, FARTHEST_Z);
    }

    reset_transform();
    set_clip_rect_only(rect);

    return true;
}

/// Reset any enabled scissor tests
///	@param rect - rectangle to clip
///	@return true on success
bool renderer::remove_clip_rect() const noexcept
{
    if (!set_current_context())
    {
        return false;
    }

    gl_call(glDisable(GL_SCISSOR_TEST));
    if (fbo_stack_.empty())
    {
        set_model_view(0, rect_);
    }
    else
    {
        const auto& top_texture = fbo_stack_.top();
        set_model_view(top_texture->get_FBO(), top_texture->get_rect());
    }

    return true;
}

/// Set scissor test rectangle
///	@param rect - rectangle to clip
///	@return true on success
bool renderer::set_clip_rect_only(const rect& rect) const noexcept
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

/// Set clipping planes
///	@param rect - rectangle to clip
///	@param transform - transformation
///	@return true on success
bool renderer::set_clip_planes(const rect& rect, const math::transformf& transform) const noexcept
{
    if(!set_current_context())
    {
        return false;
    }

    auto from_point_normal = [](const math::vec2& point, const math::vec2& normal) -> math::tvec4<double> {
        math::vec2 normalized_normal = glm::normalize(normal);
        math::vec4 result(normalized_normal.x, normalized_normal.y, 0.0f,
                          -glm::dot(point, normalized_normal));
        return result;
    };

    auto plane0 = from_point_normal({rect.x, rect.y}, {1.0f, 0.0f});
    auto plane1 = from_point_normal({rect.x + rect.w, rect.y}, {0.0f, 1.0f});
    auto plane2 = from_point_normal({rect.x + rect.w, rect.y + rect.h}, {-1.0f, 0.0f});
    auto plane3 = from_point_normal({rect.x, rect.y + rect.h}, {0.0f, -1.0f});

    // Enable each clipping plane
    gl_call(glEnable(GL_CLIP_PLANE0));
    gl_call(glEnable(GL_CLIP_PLANE1));
    gl_call(glEnable(GL_CLIP_PLANE2));
    gl_call(glEnable(GL_CLIP_PLANE3));

    push_transform(transform);
    gl_call(glClipPlane(GL_CLIP_PLANE0, math::value_ptr(plane0)));
    gl_call(glClipPlane(GL_CLIP_PLANE1, math::value_ptr(plane1)));
    gl_call(glClipPlane(GL_CLIP_PLANE2, math::value_ptr(plane2)));
    gl_call(glClipPlane(GL_CLIP_PLANE3, math::value_ptr(plane3)));
    pop_transform();

    return true;
}

/// Disable clipping planes
///	@return true on success
bool renderer::remove_clip_planes() const noexcept
{
    if (!set_current_context())
    {
        return false;
    }

    // Disable each clipping plane
    gl_call(glDisable(GL_CLIP_PLANE0));
    gl_call(glDisable(GL_CLIP_PLANE1));
    gl_call(glDisable(GL_CLIP_PLANE2));
    gl_call(glDisable(GL_CLIP_PLANE3));

    return true;
}

void renderer::set_line_width(float width) const noexcept
{
    if(math::epsilonEqual(width, 0.0f, math::epsilon<float>()))
    {
        return;
    }

    width = std::max(1.0f, width);
    gl_call(glLineWidth(width));
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
    resize(win_.get_size().w, win_.get_size().h);

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

    const auto vtx_stride = sizeof(decltype(list.vertices)::value_type);
    const auto vertices_mem_size = list.vertices.size() * vtx_stride;
    const auto idx_stride = sizeof(decltype(list.indices)::value_type);
    const auto indices_mem_size = list.indices.size() * idx_stride;

    // Upload vertices to VRAM
    if (!master_vbo_.update(list.vertices.data(), 0, vertices_mem_size))
    {
        // We're out of vertex budget. Allocate a new vertex buffer
        master_vbo_.reserve(list.vertices.data(), vertices_mem_size, true);
    }

    // Upload indices to VRAM
    if (!master_ibo_.update(list.indices.data(), 0, indices_mem_size))
    {
        // We're out of index budget. Allocate a new index buffer
        master_ibo_.reserve(list.indices.data(), indices_mem_size, true);
    }

    // Bind the vertex buffer
    master_vbo_.bind();

    // Bind the index buffer
    master_ibo_.bind();

    set_blending_mode(blending_mode::blend_normal);

    const auto& projection = current_ortho_ * transforms_.top();
    // Draw commands
    for (const auto& cmd : list.commands)
    {

        if(cmd.setup.program.shader)
        {
            cmd.setup.program.shader->enable();
        }

        if(cmd.setup.program.layout)
        {
            cmd.setup.program.layout.bind();
        }

        if(cmd.setup.program.shader->has_uniform("uProjection"))
        {
            cmd.setup.program.shader->set_uniform("uProjection", projection);
        }

        if(cmd.setup.begin)
        {
            cmd.setup.begin(gpu_context{*this, cmd.setup.program});
        }

        if (cmd.clip_rect)
        {
            set_clip_rect_only(cmd.clip_rect);
        }

        gl_call(glDrawElements(to_gl_primitive(cmd.type), GLsizei(cmd.indices_count), get_index_type(),
                               reinterpret_cast<const GLvoid*>(uintptr_t(cmd.indices_offset * idx_stride))));

        if (cmd.clip_rect)
        {
            remove_clip_rect();
        }

        if (cmd.setup.end)
        {
            cmd.setup.end(gpu_context{*this, cmd.setup.program});
        }

        if(cmd.setup.program.layout)
        {
            cmd.setup.program.layout.unbind();
        }

        if(cmd.setup.program.shader)
        {
            cmd.setup.program.shader->disable();
        }

    }

    set_blending_mode(blending_mode::blend_none);

    master_vbo_.unbind();
    master_ibo_.unbind();

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
