#pragma once

#include "color.h"
#include "point.h"
#include <cstdint>
#include <vector>

namespace video_ctrl
{

/// Common vertex definition attribute properties
struct vertex_buffer_element
{
    std::string atrr;
    std::uint32_t count = 0;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    std::uint32_t attr_type = 0;
    std::int32_t location = -1;
    bool normalized = false;
};

/// Vertex buffer layout helper structure
/// for building vertex buffers
class vertex_buffer_layout
{
public:
    void set_program_id(uint32_t id) noexcept;
    void set_stride(uint32_t stride) noexcept;

    template <typename T>
    void add(uint8_t count, uint8_t offset, const std::string& attr, bool normalized = false);
    void bind() const noexcept;
    void unbind() const noexcept;

    inline operator bool() const noexcept
    {
        return id_ != 0 && !elements_.empty();
    }

private:
    uint32_t id_ = 0;
    std::vector<vertex_buffer_element> elements_;
    uint32_t stride_ = 0;
};

/// Specializations for pushing data to vertex_buffer_layout
template <>
void vertex_buffer_layout::add<float>(uint8_t count, uint8_t offset, const std::string& attr, bool normalized);
template <>
void vertex_buffer_layout::add<uint32_t>(uint8_t count, uint8_t offset, const std::string& attr, bool normalized);
template <>
void vertex_buffer_layout::add<uint8_t>(uint8_t count, uint8_t offset, const std::string& attr, bool normalized);

/// A common vertex definition
struct vertex_2d
{
    vertex_2d() = default;
    vertex_2d(math::vec2 p, math::vec2 tex_coords, color c) noexcept
        : pos(p), uv(tex_coords), col(c)
    {}
    math::vec2 pos; // 2d position
    math::vec2 uv;  // 2d texture coordinates
    color col;      // 32bit RGBA color
};

/// A vertex buffer object wrapper
class vertex_buffer
{
public:
    vertex_buffer();
    ~vertex_buffer();

    /// Create a vertex buffer
    void create() noexcept;

    /// Destroy a vertex buffer
    void destroy() noexcept;

    /// Reallocate the vertex buffer
    void reserve(const void* data, std::size_t size, bool dynamic = false) const noexcept;

    /// Upload new vertices to the vertex buffer
    bool update(const void* data, std::size_t offset, std::size_t size) const noexcept;

    void bind() const noexcept;
    void unbind() const noexcept;

private:
    mutable std::size_t reserved_bytes_ = 0;
    uint32_t id_ = 0;
};

/// A index buffer object wrapper
class index_buffer
{
public:
    index_buffer();
    ~index_buffer();

    /// Create a index_buffer
    void create() noexcept;

    /// Destroy a index_buffer
    void destroy() noexcept;

    /// Reallocate the index_buffer
    void reserve(const void* data, std::size_t size, bool dynamic = false) const noexcept;

    /// Upload new vertices to the index_buffer
    bool update(const void* data, std::size_t offset, std::size_t size) const noexcept;

    void bind() const noexcept;
    void unbind() const noexcept;

private:
    mutable std::size_t reserved_bytes_ = 0;
    uint32_t id_ = 0;
};

} // namespace video_ctrl
