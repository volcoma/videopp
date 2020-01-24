#pragma once

#include "rect.h"
#include "vertex.h"
#include "shader.h"
#include "font.h"

#include <array>
#include <cstdint>
#include <vector>
#include <functional>

namespace gfx
{

enum programs : uint32_t
{
    simple,
    multi_channel,
    multi_channel_crop,
    single_channel,
    single_channel_crop,
    distance_field,
    distance_field_crop,
    blur,
    fxaa
};


/// Types of primitives to draw
enum class primitive_type
{
    triangles,
    triangle_fan,
    triangle_strip,
    lines,
    lines_loop,
};
/// Types of primitives to draw
enum class draw_type
{
    elements,
    array,
};


struct gpu_program
{
    gfx::shader* shader{};
};

template<size_t T>
inline gpu_program& get_program() noexcept
{
    static gpu_program program;
    return program;
}

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
    uint64_t uniforms_hash{};
};

/// A draw command
struct draw_cmd
{
    /// Type of primitive we're drawing
    primitive_type type{primitive_type::triangles};
    /// Type of draw method
    draw_type dr_type{draw_type::elements};
    /// Type of blend mode
    blending_mode blend{blending_mode::blend_none};
    /// Starting index for the index buffer buffer
    std::uint32_t indices_offset{0};
    /// Number of indices to draw
    std::uint32_t indices_count{0};
    /// Starting index for the vertex buffer
    std::uint32_t vertices_offset{0};
    /// Number of vertices to draw
    std::uint32_t vertices_count{0};
    /// Clipping rectangle
    rect clip_rect{0, 0, 0, 0};
    /// Program setup
    program_setup setup{};
    /// Uniforms's hash used for batching.
    uint64_t hash{0};
};

inline font_ptr& default_font() noexcept
{
    static font_ptr font;
    return font;
}

}


