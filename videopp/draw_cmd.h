#pragma once

#include "rect.h"
#include "texture.h"
#include "vertex.h"
#include "shader.h"
#include "font.h"

#include <array>
#include <cstdint>
#include <vector>
#include <functional>

namespace video_ctrl
{

/// Types of primitives to draw
enum class primitive_type
{
    triangles,
    triangle_fan,
    triangle_strip,
    lines,
    lines_loop,
};

/// A non-owning texture representation
struct texture_view
{
    texture_view() = default;
    texture_view(const texture_ptr& texture);
    texture_view(std::uint32_t tex_id, std::uint32_t tex_width = 0, std::uint32_t tex_height = 0);
    /// Check if texture representation is valid
    bool is_valid() const;
    operator bool() const;

    /// Compare textures by id and size
    bool operator==(const texture_view& rhs) const;
    void* get() const;

    /// Create a texture representation from a loaded texture
    static texture_view create(const texture_ptr& texture);
    static texture_view create(std::uint32_t tex_id, std::uint32_t tex_width = 0, std::uint32_t tex_height = 0);

    std::uint32_t width = 0;    // texture width
    std::uint32_t height = 0;   // texture height
    std::uint32_t id = 0;       // internal VRAM texture id
};

struct gpu_program
{
    shader_ptr shader{};
    vertex_buffer_layout layout{};
};

struct gpu_context
{
    const renderer& rend;
    const gpu_program& program;
};

struct program_setup
{
    gpu_program program;
    std::function<void(const gpu_context&)> begin;
    std::function<void(const gpu_context&)> end;

    // this is used for batching
    std::function<uint64_t()> calc_uniforms_hash;
};

/// A draw command
struct draw_cmd
{
    // Type of primitive we're drawing
    primitive_type type{primitive_type::triangles};
    // Starting index for the vertex buffer
    std::uint32_t indices_offset{0};
    // Number of indices to draw
    std::uint32_t indices_count{0};
    // Clipping rectangle
    rect clip_rect{0, 0, 0, 0};
    // Program setup
    program_setup setup{};
    // Uniforms's hash used for batching.
    uint64_t hash{0};
};

gpu_program& simple_program();
gpu_program& multi_channel_texture_program();
gpu_program& single_channel_texture_program();
gpu_program& distance_field_font_program();
gpu_program& blur_program();
gpu_program& fxaa_program();
font_ptr& default_font();
}

namespace std
{
    template<> struct hash<video_ctrl::texture_view>
    {
        using argument_type = video_ctrl::texture_view;
        using result_type = std::size_t;
        result_type operator()(argument_type const& s) const noexcept
        {
            uint64_t seed{0};
            utils::hash(seed, s.id);
            return seed;
        }
    };
}

