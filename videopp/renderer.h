#pragma once

#include <string>
#include <stack>

#include "color.h"
#include "draw_list.h"
#include "rect.h"
#include "shader.h"
#include "surface.h"
#include "texture.h"
#include "font_ptr.h"

#include <ospp/window.h>

namespace gfx
{
struct font_info;

struct gpu_stats
{
    void record(const draw_list& list);
    std::string to_string() const;

    size_t requested_calls{};
    size_t requested_opaque_calls{};
    size_t requested_blended_calls{};

    size_t rendered_calls{};
    size_t rendered_opaque_calls{};
    size_t rendered_blended_calls{};

    size_t batched_calls{};
    size_t batched_opaque_calls{};
    size_t batched_blended_calls{};

    size_t vertices{};
    size_t indices{};
};

class renderer;

struct frame_callbacks
{
    std::function<void(renderer&)> on_start_frame {};
    std::function<void(renderer&)> on_end_frame {};
};

class renderer
{
public:
    using ptr = std::shared_ptr<renderer>;
    using weak_ptr = std::weak_ptr<renderer>;
    using u_ptr = std::unique_ptr<renderer>;



    renderer(os::window& win, bool vsync, frame_callbacks fn = {});

    texture_ptr create_texture(const surface& surface, bool empty = false) const noexcept;
    texture_ptr create_texture(const surface& surface, size_t level_id, size_t layer_id = 0, size_t face_id = 0) const noexcept;
    texture_ptr create_texture(const std::string& file_name) const noexcept;
    texture_ptr create_texture(int w, int h, pix_type pixel_type, texture::format_type format_tp) const noexcept;
    texture_ptr create_texture(const texture_src_data& data) const noexcept;

    // Create shader for unique use with these functions
    shader_ptr create_shader(const char* fragment_code , const char* vertex_code) const noexcept;

    // comsumes font_info
    font_ptr create_font(font_info&& info, bool embedded = false) const noexcept;

    // Bind textures
    bool set_texture(texture_view texture, uint32_t id = 0) const noexcept;
    void reset_texture(uint32_t id = 0) const noexcept;

    void set_texture_sampler(texture_view texture, uint32_t id) const noexcept;
    void reset_texture_sampler(uint32_t id) const noexcept;

    bool bind_pixmap(const pixmap& p) const noexcept;
    void unbind_pixmap(const pixmap& p) const noexcept;
    texture_ptr blur(const texture_ptr& texture, uint32_t passes = 2);

    // Transformation
    bool push_transform(const math::transformf& transform) const noexcept;
    bool pop_transform() const noexcept;
    bool reset_transform() const noexcept;
    math::transformf get_transform() const noexcept;

    // Destinations
    bool push_fbo(const texture_ptr& texture);
    bool pop_fbo();
    bool reset_fbo();
    bool is_with_fbo() const;

    void present() noexcept;
    void present(const size& new_win_size) noexcept;
    void clear(const color& color = {}) const noexcept;

    // VSync
    bool set_vsync(bool vsync) noexcept;
    bool enable_vsync() noexcept;
    bool disable_vsync() noexcept;

    const rect& get_rect() const;
    const gpu_stats& get_stats() const;

    ~renderer();
    renderer(const renderer& other) = delete;
    renderer(renderer&& other) = delete;

    draw_list& get_list() const noexcept;
    void flush() const noexcept;
    void check_stacks() const noexcept;
    void delete_textures() noexcept;
private:

    using transform_stack = std::stack<math::mat4x4>;

    transform_stack& get_transform_stack() const noexcept;
    void queue_to_delete_texture(pixmap pixmap_id, uint32_t fbo_id, uint32_t texture_id) const;

    // Set blending
    bool set_blending_mode(blending_mode mode) const noexcept;
    blending_mode get_apropriate_blend_mode(blending_mode mode, const gpu_program& program) const noexcept;

    bool draw_cmd_list(const draw_list& list) const noexcept;
    bool push_clip(const rect& rect) const noexcept;
    bool pop_clip() const noexcept;

    friend class texture;
    friend class shader;

    struct fbo_context
    {
        texture_ptr fbo;
        draw_list list;
        transform_stack transforms;
    };

    os::window& win_;
    std::unique_ptr<context> context_;
    rect rect_;

    mutable rect transformed_rect_ {};
    mutable std::vector<pixmap> pixmap_to_delete_ {};
    mutable std::vector<uint32_t> fbo_to_delete_ {};
    mutable std::vector<uint32_t> textures_to_delete_ {};

    mutable math::mat4x4 current_ortho_;
    mutable std::stack<fbo_context> fbo_stack_;
    mutable transform_stack master_transforms_;
    mutable draw_list master_list_;
    draw_list dummy_list_;

    bool vsync_enabled_ {false};

    static constexpr size_t max_buffers{3};
    mutable size_t current_vao_idx_{};
    std::array<vertex_array_object, max_buffers> stream_vaos_;
    mutable size_t current_vbo_idx_{};
    std::array<vertex_buffer, max_buffers> stream_vbos_;
    mutable size_t current_ibo_idx_{};
    std::array<index_buffer, max_buffers> stream_ibos_;

    mutable std::vector<shader_ptr> embedded_shaders_;
    mutable std::vector<font_ptr> embedded_fonts_;

    mutable gpu_stats stats_{};
    mutable gpu_stats last_stats_{};

    frame_callbacks frame_callbacks_ {};

    constexpr static auto wrap_count{size_t(texture::wrap_type::count)};
    constexpr static auto interp_count{size_t(texture::interpolation_type::count)};

    std::array<std::array<uint32_t, interp_count>, wrap_count> samplers_{{}};

    void setup_sampler(texture::wrap_type wrap, texture::interpolation_type interp) noexcept;
    void set_model_view(uint32_t model, const rect& rect) const noexcept;
    bool set_current_context() const noexcept;
    void set_old_framebuffer() const noexcept;
    void resize(int new_width, int new_height) noexcept;
};
}
