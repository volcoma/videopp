#include "renderer.h"

#include "font.h"
#include "ttf_font.h"
#include "texture.h"
#include "logger.h"
#include "detail/shaders.h"
#include "detail/utils.h"
#include <set>

#ifdef WGL_CONTEXT
#include "detail/wgl/context_wgl.h"
#elif GLX_CONTEXT
#include "detail/glx/context_glx.h"
#elif EGL_CONTEXT
#include "detail/egl/context_egl.h"
#else
#endif


namespace gfx
{

namespace
{
    struct gl_module
    {
        gl_module()
        {
            if(!gladLoad())
            {
                throw exception("Could not open gl module");
            }
        }
        ~gl_module()
        {
            gladUnload();
        }
    } module;


    constexpr float FARTHEST_Z = -1.0f;

    // Callback function for printing debug statements
    void APIENTRY MessageCallback(GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei,
                                const GLchar *msg, const void*)
    {
        std::string source_str;
        std::string type_str;
        std::string severity_str;

        switch (source) {
            case GL_DEBUG_SOURCE_API:
            source_str = "api";
            break;

            case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            source_str = "window system";
            break;

            case GL_DEBUG_SOURCE_SHADER_COMPILER:
            source_str = "shader compiler";
            break;

            case GL_DEBUG_SOURCE_THIRD_PARTY:
            source_str = "THIRD PARTY";
            break;

            case GL_DEBUG_SOURCE_APPLICATION:
            source_str = "application";
            break;

            case GL_DEBUG_SOURCE_OTHER:
            source_str = "unknown";
            break;

            default:
            source_str = "unknown";
            break;
        }

        switch (type) {
            case GL_DEBUG_TYPE_ERROR:
            type_str = "error";
            break;

            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            type_str = "deprecated behavior";
            break;

            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            type_str = "undefined behavior";
            break;

            case GL_DEBUG_TYPE_PORTABILITY:
            type_str = "portability";
            break;

            case GL_DEBUG_TYPE_PERFORMANCE:
            type_str = "performance";
            break;

            case GL_DEBUG_TYPE_OTHER:
            type_str = "other";
            break;

            case GL_DEBUG_TYPE_MARKER:
            type_str = "unknown";
            break;

            default:
            type_str = "unknown";
            break;
        }

        switch (severity) {
            case GL_DEBUG_SEVERITY_HIGH:
            severity_str = "high";
            break;

            case GL_DEBUG_SEVERITY_MEDIUM:
            severity_str = "medium";
            break;

            case GL_DEBUG_SEVERITY_LOW:
            severity_str = "low";
            break;

            case GL_DEBUG_SEVERITY_NOTIFICATION:
            severity_str = "info";
            break;

            default:
            severity_str = "unknown";
            break;
        }

        std::stringstream ss;
        ss << "--[OPENGL CALLBACK]--\n"
           << "   source   : " << source_str << "\n"
           << "   type     : " << type_str << "\n"
           << "   severity : " << severity_str << "\n"
           << "   id       : " << id << "\n"
           << "   message  : " << msg << "\n";

        if(severity > GL_DEBUG_SEVERITY_NOTIFICATION)
        {
            log(ss.str());
        }


        assert(type != GL_DEBUG_TYPE_ERROR);
    }

    rect transform_rect(const rect& rect, const math::transformf& transform) noexcept
    {
        const auto& scale = transform.get_scale();

        return { int(std::lround(float(rect.x) * scale.x)), int(std::lround(float(rect.y) * scale.y)),
                 int(std::lround(float(rect.w) * scale.x)), int(std::lround(float(rect.h) * scale.y))};
    }

    rect inverse_and_tranfrom_rect(const rect& rect, const math::transformf& transform) noexcept
    {
        auto inversed_transform = math::inverse(transform);
        return transform_rect(rect, inversed_transform);
    }

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
}

/// Construct the renderer and initialize default rendering states
///	@param win - the window handle
///	@param vsync - true to enable vsync
renderer::renderer(os::window& win, bool vsync, frame_callbacks fn)
    : win_(win)
#ifdef WGL_CONTEXT
    , context_(std::make_unique<context_wgl>(win.get_native_handle()))
#elif GLX_CONTEXT
    , context_(std::make_unique<context_glx>(win.get_native_handle(), win.get_native_display()/*, 3, 3*/))
#elif EGL_CONTEXT
    , context_(std::make_unique<context_egl>(win.get_native_handle(), win.get_native_display()/*, 3*/))
#endif
    , rect_({0, 0, int(win.get_size().w), int(win_.get_size().h)})
{
    if(!gladLoadGL())
    {
        throw exception("Cannot load glad.");
    }

    GLint max_tex_units{};
    gl_call(glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_tex_units));

    draw_config cfg;
    cfg.max_textures_per_batch = max_tex_units;
    set_draw_config(cfg);

    log("Max Texture Units Supported : " + std::to_string(max_tex_units));
    log("Max Texture Units Per Batch configured to : " + std::to_string(cfg.max_textures_per_batch));

    context_->set_vsync(vsync);

    // During init, enable debug output
    gl_call(glEnable(GL_DEBUG_OUTPUT));
    gl_call(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
    gl_call(glDebugMessageCallback(MessageCallback, nullptr));

    // Depth calculation
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
    for(auto& vao : stream_vaos_)
    {
        vao.create();
    }
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

    auto create_program = [&](auto& program, auto fs, auto vs)
    {
        if(!program.shader)
        {
            auto shader = create_shader(fs, vs);
            embedded_shaders_.emplace_back(shader);
            program.shader = shader.get();
            auto& layout = shader->get_layout();
            constexpr auto stride = sizeof(vertex_2d);
            layout.template add<float>(2, offsetof(vertex_2d, pos), "aPosition", stride);
            layout.template add<float>(2, offsetof(vertex_2d, uv), "aTexCoord", stride);
            layout.template add<uint8_t>(4, offsetof(vertex_2d, col) ,"aColor", stride, true);
            layout.template add<uint8_t>(4, offsetof(vertex_2d, extra_col) , "aExtraColor", stride, true);
            layout.template add<float>(2, offsetof(vertex_2d, extra_data), "aExtraData", stride);
            layout.template add<uint32_t>(1, offsetof(vertex_2d, tex_idx), "aTexIndex", stride);

        }
    };

    create_program(get_program<programs::simple>(),
                   std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(fs_simple).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::multi_channel>(),
                   std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(fs_multi_channel).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::multi_channel_crop>(),
                   std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(user_defines)
                       .append(fs_multi_channel).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::single_channel>(),
                   std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(fs_single_channel).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::single_channel_crop>(),
                   std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(user_defines)
                       .append(fs_single_channel).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::distance_field>(),
                   std::string(glsl_version)
                       .append(glsl_derivatives)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(fs_distance_field).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::distance_field_crop>(),
                   std::string(glsl_version)
                       .append(glsl_derivatives)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(user_defines)
                       .append(fs_distance_field).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());


    create_program(get_program<programs::distance_field_supersample>(),
                   std::string(glsl_version)
                       .append(glsl_derivatives)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(supersample)
                       .append(fs_distance_field).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::distance_field_crop_supersample>(),
                   std::string(glsl_version)
                       .append(glsl_derivatives)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(user_defines)
                       .append(supersample)
                       .append(fs_distance_field).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::alphamix>(),
                    std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(fs_alphamix).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::valphamix>(),
                    std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(fs_valphamix).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::halphamix>(),
                    std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(fs_halphamix).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::raw_alpha>(),
                    std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(fs_raw_alpha).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::grayscale>(),
                    std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(fs_grayscale).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    create_program(get_program<programs::blur>(),
                   std::string(glsl_version)
                       .append(glsl_precision)
                       .append(common_funcs)
                       .append(fs_blur).c_str(),
                   std::string(glsl_version)
                       .append(vs_simple).c_str());

    if(!font_default())
    {
        font_default() = create_font(create_default_font(13), true);
    }

    master_list_.reserve_rects(4096);
    dummy_list_.add_line({0, 0}, {1, 1}, {0, 0, 0, 0});

    setup_sampler(texture::wrap_type::wrap_clamp, texture::interpolation_type::interpolate_linear);
    setup_sampler(texture::wrap_type::wrap_repeat, texture::interpolation_type::interpolate_linear);
    setup_sampler(texture::wrap_type::wrap_mirror, texture::interpolation_type::interpolate_linear);
    setup_sampler(texture::wrap_type::wrap_clamp, texture::interpolation_type::interpolate_none);
    setup_sampler(texture::wrap_type::wrap_repeat, texture::interpolation_type::interpolate_none);
    setup_sampler(texture::wrap_type::wrap_mirror, texture::interpolation_type::interpolate_none);

    clear(color::black());
    present();
    clear(color::black());

    frame_callbacks_ = std::move(fn);
    if (frame_callbacks_.on_start_frame)
    {
        frame_callbacks_.on_start_frame(*this);
    }
}

void renderer::setup_sampler(texture::wrap_type wrap, texture::interpolation_type interp) noexcept
{
    uint32_t sampler_state = 0;
    gl_call(glGenSamplers(1, &sampler_state));
    switch (wrap) {
    case texture::wrap_type::wrap_mirror:
        gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT));
        gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT));
        break;
    case texture::wrap_type::wrap_repeat:
        gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_S, GL_REPEAT));
        gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_T, GL_REPEAT));
        break;
    case texture::wrap_type::wrap_clamp:
    default:
        gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        break;
    }

    switch (interp) {
    case texture::interpolation_type::interpolate_none:
        gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        break;
    case texture::interpolation_type::interpolate_linear:
    default:
        gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        gl_call(glSamplerParameteri(sampler_state, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        break;
    }

    samplers_[size_t(wrap)][size_t(interp)] = sampler_state;
}

renderer::~renderer()
{
    set_current_context();
    delete_textures();
    embedded_fonts_.clear();
    embedded_shaders_.clear();

    auto clear_embedded = [](auto& shared)
    {
        if(shared.use_count() == 1)
        {
            shared.reset();
        }
    };

    clear_embedded(font_default());
    clear_embedded(font_regular());
    clear_embedded(font_bold());
    clear_embedded(font_black());
    clear_embedded(font_monospace());
}

draw_list& renderer::get_list() const noexcept
{
    if (fbo_stack_.empty())
    {
        return master_list_;
    }

    return fbo_stack_.top().list;
}

renderer::transform_stack& renderer::get_transform_stack() const noexcept
{
    if (fbo_stack_.empty())
    {
        return master_transforms_;
    }

    return fbo_stack_.top().transforms;
}

void renderer::flush() const noexcept
{
    draw_cmd_list(get_list());
    get_list().clear();
}

void renderer::check_stacks() const noexcept
{
    assert(fbo_stack_.empty() && "fbo stack was not popped");
}

void renderer::queue_to_delete_texture(pixmap pixmap_id, uint32_t fbo_id, uint32_t texture_id) const
{
    if (pixmap_id != pixmap_invalid_id)
    {
        pixmap_to_delete_.push_back(pixmap_id);
    }

    if (fbo_id > 0)
    {
        fbo_to_delete_.push_back(fbo_id);
    }

    if (texture_id > 0)
    {
        textures_to_delete_.push_back(texture_id);
    }
}

void renderer::delete_textures() noexcept
{
    for (auto& pixmap_id : pixmap_to_delete_)
    {
        context_->destroy_pixmap(pixmap_id);
    }
    pixmap_to_delete_.clear();

    if (!fbo_to_delete_.empty())
    {
        gl_call(glDeleteFramebuffers(GLsizei(fbo_to_delete_.size()), fbo_to_delete_.data()));
        fbo_to_delete_.clear();
    }

    if (!textures_to_delete_.empty())
    {
        // Destroy texture
        gl_call(glDeleteTextures(GLsizei(textures_to_delete_.size()), textures_to_delete_.data()));
        textures_to_delete_.clear();
    }
}

/// Bind FBO, set projection, switch to modelview
///	@param fbo - framebuffer object, use 0 for the backbuffer
///	@param rect - the dirty area to draw into
void renderer::set_model_view(uint32_t fbo, const rect& rect) const noexcept
{
    // Bind the FBO or backbuffer if 0
    gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    // Set the viewport
    gl_call(glViewport(0, 0, rect.w, rect.h));

    // Set the projection
    if(fbo == 0)
    {
        // If we have 0 it is the back buffer
        current_ortho_ = math::ortho<float>(0, float(rect.w), float(rect.h), 0, 0, FARTHEST_Z);
    }
    else
    {
        // If we have > 0 the fbo must be flipped
        current_ortho_ = math::ortho<float>(0, float(rect.w), 0, float(rect.h), 0, FARTHEST_Z);
    }

//    // Switch to modelview matrix and setup identity
//    reset_transform();
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
    gl_call(glBindFramebuffer(GL_FRAMEBUFFER, top_texture.fbo->get_FBO()));
}

/// Resize the dirty rectangle
/// @param new_width - the new width
/// @param new_height - the new height
void renderer::resize(int new_width, int new_height) noexcept
{
    rect_.w = new_width;
    rect_.h = new_height;
}

texture_ptr renderer::create_texture(const surface& surface, bool empty) const noexcept
{
    if(!set_current_context())
    {
        return {};
    }

    try
    {
        auto tex = new texture(*this, surface, empty);
        return texture_ptr(tex);
    }
    catch(const exception& e)
    {
        log(std::string("ERROR: Cannot create texture from surface. Reason ") + e.what());
    }

    return texture_ptr(nullptr);
}

texture_ptr renderer::create_texture(const surface& surface, size_t level_id, size_t layer_id, size_t face_id) const noexcept
{
    if(!set_current_context())
    {
        return {};
    }

    try
    {
        auto tex = new texture(*this, surface, level_id, layer_id, face_id);
        return texture_ptr(tex);
    }
    catch(const exception& e)
    {
        log(std::string("ERROR: Cannot create texture from surface. Reason ") + e.what());
    }

    return texture_ptr(nullptr);
}

texture_ptr renderer::create_texture(const std::string& file_name) const noexcept
{
    if(!set_current_context())
    {
        return {};
    }

    try
    {
        surface surf(file_name);
        auto tex = new texture(*this, surf);
        texture_ptr texture(tex);

        return texture;
    }
    catch(const exception& e)
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
        auto tex = new texture(*this, w, h, pixel_type, format_type);
        return texture_ptr(tex);
    }
    catch(const exception& e)
    {
        log(std::string("ERROR: Cannot create blank texture. Reason ") + e.what());
    }

    return texture_ptr();
}

texture_ptr renderer::create_texture(const texture_src_data& data) const noexcept
{
    if(!set_current_context())
    {
        return {};
    }

    try
    {
        auto tex = new texture(*this, data);
        return texture_ptr(tex);
    }
    catch(const exception& e)
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
    catch(const exception& e)
    {
        log(std::string("ERROR: Cannot create shader. Reason ") + e.what());
    }

    return {};
}

/// Create a font
///	@param info - description of font
///	@return a shared pointer to the font
font_ptr renderer::create_font(font_info&& info, bool embedded) const noexcept
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
        if(r->sdf_spread == 0)
        {
            r->texture->generate_mipmap();
        }
        r->surface.reset();

        if(embedded)
        {
            embedded_fonts_.emplace_back(r);
        }

        return r;
    }
    catch(const exception& e)
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
            gl_call(glBlendEquation(GL_FUNC_ADD));
            gl_call(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE));
            break;
        case blending_mode::unmultiplied_alpha:
            gl_call(glEnable(GL_BLEND));
            gl_call(glBlendEquation(GL_FUNC_ADD));
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

blending_mode renderer::get_apropriate_blend_mode(blending_mode mode, const gpu_program &program) const noexcept
{
    if(!fbo_stack_.empty())
    {
        const auto& fbo = fbo_stack_.top().fbo;
        if(fbo->get_pix_type() == pix_type::rgba)
        {
            // When drawing into a fbo which has an alpha channel
            // the proper blending should be pre_multiplication
            // We do this here so that it is hidden from the end user
            if( mode == blending_mode::blend_normal &&
                program.shader != get_program<programs::raw_alpha>().shader )
            {
                mode = blending_mode::pre_multiplication;
            }
        }
    }

    return mode;
}

/// Lowest level texture binding call, for both fixed pipeline and shaders, and provide optional wrapping and interpolation modes
///     @param texture - the texture view
///     @param id - texture unit id (for the shader)
///     @param wrap_type - wrapping type
///     @param interp_type - interpolation type
bool renderer::set_texture(texture_view texture, uint32_t id) const noexcept
{
    // Activate texture for shader
    gl_call(glActiveTexture(GL_TEXTURE0 + id));

    // Bind texture to the pipeline, and the currently active texture
    gl_call(glBindTexture(GL_TEXTURE_2D, texture.id));
    return true;
}

/// Reset a texture slot
///     @param id - the texture to reset
void renderer::reset_texture(uint32_t id) const noexcept
{
    gl_call(glActiveTexture(GL_TEXTURE0 + id));
    gl_call(glBindTexture(GL_TEXTURE_2D, 0));
}

void renderer::set_texture_sampler(texture_view texture, uint32_t id) const noexcept
{
    auto sampler = samplers_[size_t(texture.wrap_type)][size_t(texture.interp_type)];
    if(sampler != 0)
    {
        gl_call(glBindSampler(id, sampler));
    }
    else
    {
        log("ERROR: Undefined sampler used! If it's legit - add it on initialization phase"); //auto add?
    }
}

void renderer::reset_texture_sampler(uint32_t id) const noexcept
{
    // Unbind active sampler
    gl_call(glBindSampler(id, 0));
}

bool renderer::bind_pixmap(const pixmap& p) const noexcept
{
    return context_->bind_pixmap(p);
}

/// Reset a texture slot
///     @param id - the texture to reset
void renderer::unbind_pixmap(const pixmap& p) const noexcept
{
    context_->unbind_pixmap(p);
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

        program_setup setup;
        setup.program = get_program<programs::blur>();
        setup.begin = [=](const gpu_context& ctx)
        {
            ctx.program.shader->set_uniform("uTexture", input);
            ctx.program.shader->set_uniform("uTextureSize", input_size);
            ctx.program.shader->set_uniform("uDirection", direction);
        };

        list.add_image(input, input->get_rect(), fbo->get_rect(), {}, color::white(), flip_format::none, setup);

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
    auto& transforms = get_transform_stack();
    if(transforms.empty())
    {
        transforms.push(transform);
    }
    else
    {
        const auto& top = transforms.top();
        transforms.push(top * transform.get_matrix());
    }

    return true;
}

/// Restore a previously set matrix
///     @return true on success
bool renderer::pop_transform() const noexcept
{
    auto& transforms = get_transform_stack();
    transforms.pop();

    if (transforms.empty())
    {
        reset_transform();
    }
    return true;
}

/// Reset the modelview transform to identity
///     @return true on success
bool renderer::reset_transform() const noexcept
{
    auto& transforms = get_transform_stack();
    transform_stack new_tranform;
    new_tranform.push(math::transformf::identity());
    transforms.swap(new_tranform);

    return true;
}

math::transformf renderer::get_transform() const noexcept
{
    auto& transforms = get_transform_stack();
    return transforms.top();
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

    auto mod_clip = rect;
    const auto& transforms = get_transform_stack();
    if (transforms.size() > 1)
    {
        const auto trans = math::transformf{transforms.top()};
        const auto& scale = trans.get_scale();
        const auto& pos = trans.get_position();
        mod_clip.x = int(float(mod_clip.x) * scale.x + pos.x);
        mod_clip.y = int(float(mod_clip.y) * scale.y + pos.y);
        mod_clip.w = int(float(rect.w) * scale.x);
        mod_clip.h = int(float(rect.h) * scale.y);
    }

    if (fbo_stack_.empty())
    {
        // If we have no fbo_target_ it is a back buffer
        gl_call(glScissor(mod_clip.x, rect_.h - mod_clip.y - mod_clip.h, mod_clip.w, mod_clip.h));
    }
    else
    {
        gl_call(glScissor(mod_clip.x, mod_clip.y, mod_clip.w, mod_clip.h));
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

    fbo_stack_.emplace();
    auto& top_texture = fbo_stack_.top();
    top_texture.fbo = texture;
    top_texture.transforms.push(math::transformf::identity());

    set_model_view(top_texture.fbo->get_FBO(), top_texture.fbo->get_rect());
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

    draw_cmd_list(fbo_stack_.top().list);
    fbo_stack_.pop();

    if (fbo_stack_.empty())
    {
        set_model_view(0, rect_);
        return true;
    }

    auto& top_texture = fbo_stack_.top();
    set_model_view(top_texture.fbo->get_FBO(), top_texture.fbo->get_rect());
    return true;
}

/// Remove all FBO targets from stack and set draw to back render buffer
///	@return true on success
bool renderer::reset_fbo()
{
    if (!set_current_context())
    {
        return false;
    }

    while (!fbo_stack_.empty())
    {
        draw_cmd_list(fbo_stack_.top().list);
        fbo_stack_.pop();
    }

    set_model_view(0, rect_);
    return true;
}

bool renderer::is_with_fbo() const
{
    return !fbo_stack_.empty();
}

/// Swap buffers
void renderer::present() noexcept
{
    auto sz = win_.get_size();
    resize(int(sz.w), int(sz.h));

//    {
//        const auto& stats = get_stats();

//        gfx::text text;
//        text.set_font(gfx::default_font(), 24);
//        text.set_utf8_text(stats.to_string());
//        text.set_outline_width(0.1f);

//        math::transformf t {};
//        t.set_position(0, 100);

//        get_list().add_text(text, t);
//    }

    if (frame_callbacks_.on_end_frame)
    {
        frame_callbacks_.on_end_frame(*this);
    }

    if(!master_list_.commands_requested)
    {
        // due to bug in the driver
        // we must keep it busy
        draw_cmd_list(dummy_list_);
    }
    else
    {
        draw_cmd_list(master_list_);
        master_list_.clear();
    }

    last_stats_ = stats_;
    stats_ = {};

    context_->swap_buffers();

    set_current_context();
    delete_textures();
    reset_transform();
    set_model_view(0, rect_);

    if (frame_callbacks_.on_start_frame)
    {
        frame_callbacks_.on_start_frame(*this);
    }
}

/// Clear the currently bound buffer
///	@param color - color to clear with
void renderer::clear(const color& color) const noexcept
{
    set_current_context();

    if (fbo_stack_.empty())
    {
        master_list_.push_blend(blending_mode::blend_none);
        master_list_.add_rect(get_rect(), color);
        master_list_.pop_blend();
        return;
    }

    auto& fbo_info = fbo_stack_.top();
    fbo_info.list.push_blend(blending_mode::blend_none);
    fbo_info.list.add_rect(fbo_info.fbo->get_rect(), color);
    fbo_info.list.pop_blend();
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
    if (list.empty())
    {
        return false;
    }

    if (!set_current_context())
    {
        return false;
    }

    stats_.record(list);

    list.validate_stacks();

//    EGT_BLOCK_PROFILING("renderer::draw_cmd_list - %d commands", int(list.commands.size()))

    const auto vtx_stride = sizeof(decltype(list.vertices)::value_type);
    const auto vertices_mem_size = list.vertices.size() * vtx_stride;
    const auto idx_stride = sizeof(decltype(list.indices)::value_type);
    const auto indices_mem_size = list.indices.size() * idx_stride;

    // We are using several stream buffers to avoid syncs caused by
    // uploading new data while the old one is still processing
    auto& vao = stream_vaos_[current_vao_idx_];
    auto& vbo = stream_vbos_[current_vbo_idx_];
    auto& ibo = stream_ibos_[current_ibo_idx_];

    current_vao_idx_ = (current_vao_idx_ + 1) % stream_vaos_.size();
    current_vbo_idx_ = (current_vbo_idx_ + 1) % stream_vbos_.size();
    current_ibo_idx_ = (current_ibo_idx_ + 1) % stream_ibos_.size();


    bool mapped = get_draw_config().mapped_buffers;
    {
//        EGT_BLOCK_PROFILING("draw_cmd_list::vao/vbo/ibo")

        // Bind the vertex array object
        vao.bind();

        // Bind the vertex buffer
        vbo.bind();

        // Upload vertices to VRAM
        if (!vbo.update(list.vertices.data(), 0, vertices_mem_size, mapped))
        {
            // We're out of vertex budget. Allocate a new vertex buffer
            vbo.reserve(list.vertices.data(), vertices_mem_size, true);
        }

        // Bind the index buffer
        ibo.bind();

        // Upload indices to VRAM
        if (!ibo.update(list.indices.data(), 0, indices_mem_size, mapped))
        {
            // We're out of index budget. Allocate a new index buffer
            ibo.reserve(list.indices.data(), indices_mem_size, true);
        }
    }



    {
        blending_mode last_blend{blending_mode::blend_none};
        set_blending_mode(last_blend);

        // Draw commands
        for (const auto& cmd : list.commands)
        {
//            EGT_BLOCK_PROFILING("draw_cmd_list::cmd")

            {
                if(cmd.setup.program.shader)
                {
//                    EGT_BLOCK_PROFILING("draw_cmd_list::shader.enable")

                    cmd.setup.program.shader->enable();
                }

                if (cmd.clip_rect)
                {
                    push_clip(cmd.clip_rect);
                }

                if(cmd.setup.begin)
                {
//                    EGT_BLOCK_PROFILING("draw_cmd_list::cmd.setup.begin")

                    cmd.setup.begin(gpu_context{cmd, *this, cmd.setup.program});
                }

                if(cmd.setup.program.shader && cmd.setup.program.shader->has_uniform("uProjection"))
                {
                    const auto& projection = current_ortho_ * get_transform_stack().top();
                    cmd.setup.program.shader->set_uniform("uProjection", projection);
                }

                if (cmd.blend != last_blend)
                {
                    auto mode = get_apropriate_blend_mode(cmd.blend, cmd.setup.program);
                    set_blending_mode(mode);
                    last_blend = mode;
                }
            }

            switch(cmd.dr_type)
            {
                case draw_type::elements:
                {
//                    EGT_BLOCK_PROFILING("draw_cmd_list::glDrawElements - %d indices", int(cmd.indices_count))

                    gl_call(glDrawElements(to_gl_primitive(cmd.type), GLsizei(cmd.indices_count), get_index_type(),
                                           reinterpret_cast<const GLvoid*>(uintptr_t(cmd.indices_offset * idx_stride))));
                }
                break;

                case draw_type::array:
                {
//                    EGT_BLOCK_PROFILING("draw_cmd_list::glDrawArrays")

                    gl_call(glDrawArrays(to_gl_primitive(cmd.type), GLint(cmd.vertices_offset), GLsizei(cmd.vertices_count)));
                }
                break;

                default:
                break;
            }


            {

                if (cmd.setup.end)
                {
//                    EGT_BLOCK_PROFILING("draw_cmd_list::cmd.setup.end")

                    cmd.setup.end(gpu_context{cmd, *this, cmd.setup.program});
                }

                if (cmd.clip_rect)
                {
                    pop_clip();
                }

                if(cmd.setup.program.shader)
                {
//                    EGT_BLOCK_PROFILING("draw_cmd_list::shader.disable")

                    cmd.setup.program.shader->disable();
                }
            }
        }

        set_blending_mode(blending_mode::blend_none);
    }

    vbo.unbind();
    ibo.unbind();

    vao.unbind();

    if(list.debug)
    {
        draw_cmd_list(*list.debug);
    }

    return true;
}

/// Enable vertical synchronization to avoid tearing
///     @return true on success
bool renderer::enable_vsync() noexcept
{
    return context_->set_vsync(true);
}

/// Disable vertical synchronization
///     @return true on success
bool renderer::disable_vsync() noexcept
{
    return context_->set_vsync(false);
}


bool renderer::set_vsync(bool vsync) noexcept
{
    return context_->set_vsync(vsync);
}

/// Get the dirty rectangle we're drawing into
///     @return the currently used rect
const rect& renderer::get_rect() const
{
    if (fbo_stack_.empty())
    {
        const auto& transforms = get_transform_stack();
        if (transforms.size() > 1) // skip identity matrix
        {
            transformed_rect_ = inverse_and_tranfrom_rect(rect_, math::transformf{transforms.top()});
            return transformed_rect_;
        }

        return rect_;
    }

    const auto& top_stack = fbo_stack_.top();
    const auto& fbo = top_stack.fbo;
    const auto& transforms = top_stack.transforms;
    if (transforms.size() > 1) // skip identity matrix
    {
        const auto& rect = fbo->get_rect();
        transformed_rect_ = inverse_and_tranfrom_rect(rect, math::transformf{transforms.top()});
        return transformed_rect_;
    }

    return fbo->get_rect();
}

const gpu_stats& renderer::get_stats() const
{
    return last_stats_;
}

void gpu_stats::record(const draw_list& list)
{
    requested_calls += size_t(list.commands_requested);
    rendered_calls += list.commands.size();
    batched_calls += size_t(list.commands_requested) - list.commands.size();
    vertices += list.vertices.size();
    indices += list.indices.size();

    size_t req_opaque_calls = 0;
    size_t rend_opaque_calls = 0;
    size_t req_blended_calls = 0;
    size_t rend_blended_calls = 0;
    for(const auto& cmd : list.commands)
    {
        if(cmd.blend == blending_mode::blend_none)
        {
            req_opaque_calls += cmd.subcount;
            rend_opaque_calls++;
        }
        else
        {
            req_blended_calls += cmd.subcount;
            rend_blended_calls++;
        }
    }

    requested_opaque_calls += req_opaque_calls;
    requested_blended_calls += req_blended_calls;
    rendered_opaque_calls += rend_opaque_calls;
    rendered_blended_calls += rend_blended_calls;

    batched_opaque_calls += req_opaque_calls - rend_opaque_calls;
    batched_blended_calls += req_blended_calls - rend_blended_calls;
}

std::string gpu_stats::to_string() const
{
    std::stringstream ss;
    ss << "Requested:" << requested_calls << "\n"
       << "   - Opaque: " << requested_opaque_calls << "\n"
       << "   - Blended: " << requested_blended_calls << "\n"
       << "Rendered:" << rendered_calls << "\n"
       << "   - Opaque: " << rendered_opaque_calls << "\n"
       << "   - Blended: " << rendered_blended_calls << "\n"
       << "Batched:" << batched_calls << "\n"
       << "   - Opaque: " << batched_opaque_calls << "\n"
       << "   - Blended: " << batched_blended_calls;

    return ss.str();
}
}
