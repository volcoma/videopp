#pragma once

#include "vertex.h"
#include "texture.h"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <array>


namespace gfx
{
    class renderer;
    struct texture_view;

    class shader
    {
    public:
        ~shader() noexcept;

        void enable() const;
        void disable() const;

        void set_uniform(const char* uniform, const texture_view& tex, uint32_t slot = 0) const;
        void set_uniform(const char* uniform, const std::array<texture_view, 32>& texures) const;

        void set_uniform(const char* uniform, int data) const;
        void set_uniform(const char* uniform, float data) const;

        void set_uniform(const char* uniform, const math::transform_t<float>& data) const;
        void set_uniform(const char* uniform, const math::transform_t<float>::mat4_t &data) const;

        void set_uniform(const char* uniform, const math::vec<2, int>& data) const;
        void set_uniform(const char* uniform, const math::vec<2, float>& data) const;

        void set_uniform(const char* uniform, const math::vec<3, int>& data) const;
        void set_uniform(const char* uniform, const math::vec<3, float>& data) const;

        void set_uniform(const char* uniform, const math::vec<4, int>& data) const;
        void set_uniform(const char* uniform, const math::vec<4, float>& data) const;
        void set_uniform(const char* uniform, const std::vector<math::vec<4, float>>& data) const;
        void set_uniform(const char* uniform, const std::vector<rect>& data) const;

        void set_uniform(const char* uniform, const color& data) const;

        bool has_uniform(const char* uniform) const;
        void clear_textures() const;
        uint32_t get_program_id() const { return program_id_; }

        vertex_buffer_layout& get_layout() { return layout_; }
        const vertex_buffer_layout& get_layout() const { return layout_; }

    private:
        int get_uniform_location(const char* uniform) const;

        friend class renderer;
        shader(const renderer &rend, const char* fragment_code, const char* vertex_code);

        void unload() noexcept;
        void compile(uint32_t shader_id);
        void link();
        void cache_uniform_locations();

        vertex_buffer_layout layout_;
        std::map<std::string, int, std::less<>> locations_;

        uint32_t program_id_ = 0;
        uint32_t fragment_shader_id_ = 0;
        uint32_t vertex_shader_id_ = 0;
        mutable int32_t max_bound_slot_ = -1;
        mutable std::array<texture_view, 32> bound_textures_{{}};

        const renderer &rend_;
    };

    using shader_ptr = std::shared_ptr<shader>;
}
