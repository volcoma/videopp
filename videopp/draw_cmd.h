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
    distance_field_supersample,
    distance_field_crop_supersample,
    alphamix,
    valphamix,
    halphamix,
    raw_alpha,
    grayscale,
    blur
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
    /// here we use NON OWNING raw pointers for optimization
    gfx::shader* shader{};

    /// reserved for future use
};


template<size_t T>
inline gpu_program& get_program() noexcept
{
    static gpu_program program;
    return program;
}

struct draw_cmd;
struct gpu_context
{
    const draw_cmd& cmd;
    const renderer& rend;
    const gpu_program& program;
};

struct program_setup
{
    gpu_program program;

    std::function<math::transformf()> get_gpu_transform;
    std::function<void(const gpu_context&)> begin;
    std::function<void(const gpu_context&)> end;

    /// this is used for batching
    uint64_t uniforms_hash{};
};

/// A draw command
struct draw_cmd
{
    uint8_t get_texture_idx(const texture_view& tex)
    {
        auto tex_idx = used_slots;

        for(uint8_t i = 0; i < used_slots; ++i)
        {
            if(texture_slots[i] == tex)
            {
                tex_idx = i;
                break;
            }
        }

        return tex_idx;
    }

    void set_texture_idx(const texture_view& tex, uint8_t tex_idx)
    {
        texture_slots[tex_idx] = tex;
        used_slots++;
    }
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

    std::array<texture_view, 32> texture_slots{{}};
    uint8_t used_slots{};
    /// Uniforms's hash used for batching.
    uint64_t hash{0};

    size_t subcount{0};

};

}


