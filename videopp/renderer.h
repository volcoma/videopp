#pragma once

#include <string>
#include <stack>
#include <cstdint>

#include "color.h"
#include "draw_list.h"
#include "rect.h"
#include "shader.h"
#include "surface.h"
#include "texture.h"
#include "font_ptr.h"
#include <ospp/window.h>

namespace video_ctrl
{

struct context
{
    virtual ~context() = default;
    virtual bool make_current() = 0;
    virtual bool swap_buffers() = 0;
    virtual bool set_vsync(bool vsync) = 0;
};

class renderer
{
public:
    renderer(os::window& win, bool vsync);

    // Create texture for unique use with these functions
    texture_ptr create_texture(const surface& surface) const noexcept;
    texture_ptr create_texture(const std::string& file_name) const noexcept;
    texture_ptr create_texture(int w, int h, pix_type pixel_type, texture::format_type format_tp) const noexcept;

    // Create shader for unique use with these functions
    shader_ptr create_shader(const char* fragment_code , const char* vertex_code) const noexcept;

    // comsumes font_info
    font_ptr create_font(font_info&& info) const noexcept;

    // Set colors & blending
    bool set_blending_mode(const blending_mode mode) const noexcept;
    
    // Bind textures
    bool set_texture(const uint32_t texture, uint32_t id = 0, 
                    texture::wrap_type wrap_type = texture::wrap_type::wrap_repeat, 
                    texture::interpolation_type interp_type = texture::interpolation_type::interpolate_linear) const noexcept;
    bool set_texture(texture_view texture, uint32_t id = 0,
                     texture::wrap_type wrap_type = texture::wrap_type::wrap_repeat, 
                     texture::interpolation_type interp_type = texture::interpolation_type::interpolate_linear) const noexcept;
    bool set_texture(const texture_ptr& texture, uint32_t id = 0, 
                     texture::wrap_type wrap_type = texture::wrap_type::wrap_repeat, 
                     texture::interpolation_type interp_type = texture::interpolation_type::interpolate_linear) const noexcept;
    void reset_texture(uint32_t id = 0) const noexcept;
    
    texture_ptr blur(const texture_ptr& texture, uint32_t passes = 2);

    // Transformation
    bool push_transform(const math::transformf& transform) const noexcept;
    bool pop_transform() const noexcept;
    bool reset_transform() const noexcept;

    // Clipping
    bool set_clip_rect(const rect& rect) const noexcept;
    bool remove_clip_rect() const noexcept;
    bool set_clip_rect_only(const rect& rect) const noexcept;
    bool set_clip_planes(const rect& rect, const math::transformf& transform) const noexcept;
    bool remove_clip_planes() const noexcept;

    void set_line_width(float width) const noexcept;
    // Destinations
    bool push_fbo(const texture_ptr& texture);
    bool pop_fbo();
    bool reset_fbo();

    void present() noexcept;
    void clear(const color& color = {}) const noexcept;

    bool draw_cmd_list(const draw_list& list) const noexcept;

    // VSync
    bool enable_vsync() noexcept;
    bool disable_vsync() noexcept;

    const rect& get_rect() const;

    renderer(const renderer& other) = delete;
    renderer(renderer&& other) = delete;
    ~renderer();

    void resize(int new_width, int new_height) noexcept;
	bool set_current_context() const noexcept;

private:
    void set_model_view(const uint32_t model, const rect& rect) const noexcept;
    void clear_fbo(uint32_t fbo_id, const color& color) const noexcept;
    void set_old_framebuffer() const noexcept;

    friend class texture;
    friend class shader;

    static constexpr int FARTHEST_Z = -1;

    os::window& win_;
    std::unique_ptr<context> context_;
    rect rect_;

    mutable size_t current_vbo_idx_{};
    std::array<vertex_buffer, 3> stream_vbos_;
    mutable size_t current_ibo_idx_{};
    std::array<index_buffer, 3> stream_ibos_;

    mutable math::mat4x4 current_ortho_;
    mutable std::stack<math::mat4x4> transforms_;
    std::stack<texture_ptr> fbo_stack_;

    std::vector<shader_ptr> embedded_shaders_;
    std::vector<font_ptr> embedded_fonts_;
};
}
