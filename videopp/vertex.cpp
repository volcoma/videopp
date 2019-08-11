#include "vertex.h"
#include "logger.h"
#include "detail/utils.h"

namespace video_ctrl
{

////
/// Vertex buffer implementation
////

// Default vertex buffer construction does nothing (currently)
vertex_buffer::vertex_buffer() = default;

// Default vertex buffer destruction deletes buffer if available
vertex_buffer::~vertex_buffer()
{
    // Dimo: this might also crash if called after gl context release
    destroy();
}

/// Create a vertex buffer
void vertex_buffer::create() noexcept
{
    if(!id_)
    {
        gl_call(glGenBuffers(1, &id_));
    }
}

/// Destroy a vertex buffer
void vertex_buffer::destroy() noexcept
{
    if(id_)
    {
        gl_call(glDeleteBuffers(1, &id_));
        id_ = 0;
    }
}

/// Reserve a vertex buffer
///     @param data - data to upload to VRAM (optional)
///     @param size - the byte budget for this buffer
///     @param dynamic - whether the buffer is optimized for frequent updates
void vertex_buffer::reserve(const void* data, std::size_t size, bool dynamic) const noexcept
{
    // glBufferData is a slow call
    // so we have two separate functions for reserve & update a segment of the buffer
    gl_call(glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(size), data,
                         dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));
    reserved_bytes_ = size;

}

/// Update a part of the buffer
///     @param as_indices - whether to bind as vertex or index buffer
///     @param data - data to upload to VRAM
///     @param offset - starting byte offset inside the VRAM
///     @param size - the number of bytes to upload
///     @returns false if the buffer doesn't have the required budget for the new upload
bool vertex_buffer::update(const void* data, std::size_t offset, std::size_t size) const noexcept
{
    if(offset + size > reserved_bytes_)
    {
        // Updating out of limits. We need to reserve a new vertex buffer of different size
        return false;
    }

    gl_call(glBufferSubData(GL_ARRAY_BUFFER, GLsizeiptr(offset),
                            GLsizeiptr(size), data));

    return true;
}

/// Bind the hardware vertex buffer to the pipe
///     @param as_indices - whether to bind as vertex or index buffer
void vertex_buffer::bind() const noexcept
{
    gl_call(glBindBuffer(GL_ARRAY_BUFFER, id_));
}

/// Unbind the vertex buffer
///     @param as_indices - whether to bind as vertex or index buffer
void vertex_buffer::unbind() const noexcept
{
    gl_call(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

// Default index_buffer construction does nothing (currently)
index_buffer::index_buffer() = default;

// Default index_buffer destruction deletes buffer if available
index_buffer::~index_buffer()
{
    // Dimo: this might also crash if called after gl context release
    destroy();
}

/// Create a index_buffer
void index_buffer::create() noexcept
{
    if(!id_)
    {
        gl_call(glGenBuffers(1, &id_));
    }
}

/// Destroy a index_buffer
void index_buffer::destroy() noexcept
{
    if(id_)
    {
        gl_call(glDeleteBuffers(1, &id_));
        id_ = 0;
    }
}

/// Reserve a index_buffer
///     @param data - data to upload to VRAM (optional)
///     @param size - the byte budget for this buffer
///     @param dynamic - whether the buffer is optimized for frequent updates
void index_buffer::reserve(const void* data, std::size_t size, bool dynamic) const noexcept
{
    // glBufferData is a slow call
    // so we have two separate functions for reserve & update a segment of the buffer
    gl_call(glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(size), data,
                         dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));
    reserved_bytes_ = size;

}

/// Update a part of the buffer
///     @param as_indices - whether to bind as vertex or index buffer
///     @param data - data to upload to VRAM
///     @param offset - starting byte offset inside the VRAM
///     @param size - the number of bytes to upload
///     @returns false if the buffer doesn't have the required budget for the new upload
bool index_buffer::update(const void* data, std::size_t offset, std::size_t size) const noexcept
{
    if(offset + size > reserved_bytes_)
    {
        // Updating out of limits. We need to reserve a new index_buffer of different size
        return false;
    }

    gl_call(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(offset),
                            GLsizeiptr(size), data));

    return true;
}

/// Bind the hardware index_buffer to the pipe
///     @param as_indices - whether to bind as vertex or index buffer
void index_buffer::bind() const noexcept
{
    gl_call(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_));
}

/// Unbind the index_buffer
///     @param as_indices - whether to bind as vertex or index buffer
void index_buffer::unbind() const noexcept
{
    gl_call(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

template <>
void vertex_buffer_layout::add<float>(uint8_t count, uint8_t offset, const std::string& attr, bool normalized)
{
    elements_.emplace_back();
    auto& element = elements_.back();
    element.atrr = attr;
    element.count = count;
    element.offset = offset;
    element.size = sizeof(GLfloat) * count;
    element.attr_type = GL_FLOAT;
    element.normalized = normalized;
    element.location = glGetAttribLocation(id_, element.atrr.c_str());
}
template <>
void vertex_buffer_layout::add<uint32_t>(uint8_t count, uint8_t offset, const std::string& attr, bool normalized)
{
    elements_.emplace_back();
    auto& element = elements_.back();
    element.atrr = attr;
    element.count = count;
    element.offset = offset;
    element.size = sizeof(GLuint) * count;
    element.attr_type = GL_UNSIGNED_INT;
    element.normalized = normalized;
    element.location = glGetAttribLocation(id_, element.atrr.c_str());
}
template <>
void vertex_buffer_layout::add<uint8_t>(uint8_t count, uint8_t offset, const std::string& attr, bool normalized)
{
    elements_.emplace_back();
    auto& element = elements_.back();
    element.atrr = attr;
    element.count = count;
    element.offset = offset;
    element.size = sizeof(GLubyte) * element.count;
    element.attr_type = GL_UNSIGNED_BYTE;
    element.normalized = normalized;
    element.location = glGetAttribLocation(id_, element.atrr.c_str());
}

void vertex_buffer_layout::bind() const noexcept
{
    for(const auto& element : elements_)
    {
        gl_call(glEnableVertexAttribArray(GLuint(element.location)));

        gl_call(glVertexAttribPointer(GLuint(element.location),
                                      GLint(element.count),
                                      GLenum(element.attr_type),
                                      GLboolean(element.normalized),
                                      GLsizei(stride_),
                                      reinterpret_cast<const GLvoid*>(uintptr_t(element.offset))));
    }
}

void vertex_buffer_layout::set_program_id(uint32_t id) noexcept
{
    id_ = id;
}

void vertex_buffer_layout::set_stride(uint32_t stride) noexcept
{
    stride_ = stride;
}

void vertex_buffer_layout::unbind() const noexcept
{
    for(const auto& el : elements_)
    {
        gl_call(glDisableVertexAttribArray(GLuint(el.location)));
    }
}
}
