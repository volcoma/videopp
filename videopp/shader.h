#pragma once

#include "vertex.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "math/transform.hpp"
#include "texture.h"


namespace video_ctrl
{
    class renderer;
    struct texture_view;

    class shader
    {
    public:
        ~shader() noexcept;

        void enable() const;
        void disable() const;

        void set_uniform(const std::string& uniform_name, const texture_view& tex, uint32_t slot = 0, texture::wrap_type wrap_type = texture::wrap_type::wrap_clamp,
                         texture::interpolation_type interp_type = texture::interpolation_type::interpolate_linear) const;
        void set_uniform(const std::string& uniform_name, const texture_ptr& tex, uint32_t slot = 0, texture::wrap_type wrap_type = texture::wrap_type::wrap_clamp,
                         texture::interpolation_type interp_type = texture::interpolation_type::interpolate_linear) const;

        void set_uniform(const std::string& uniform_name, int data) const;
        void set_uniform(const std::string& uniform_name, float data) const;

        void set_uniform(const std::string& uniform_name, const math::transform_t<float>& data) const;
        void set_uniform(const std::string& uniform_name, const math::transform_t<float>::mat4_t &data) const;

        void set_uniform(const std::string& uniform_name, const math::transform_t<int>::vec2_t& data) const;
        void set_uniform(const std::string& uniform_name, const math::transform_t<float>::vec2_t& data) const;

        void set_uniform(const std::string& uniform_name, const math::transform_t<int>::vec3_t& data) const;
        void set_uniform(const std::string& uniform_name, const math::transform_t<float>::vec3_t& data) const;

        void set_uniform(const std::string& uniform_name, const math::transform_t<int>::vec4_t& data) const;
        void set_uniform(const std::string& uniform_name, const math::transform_t<float>::vec4_t& data) const;

        void set_uniform(const std::string& uniform_name, const color& data) const;

        bool has_uniform(const std::string& uniform_name) const;
        void clear_textures() const;
        uint32_t get_program_id() const { return program_id_; }

        vertex_buffer_layout& get_layout() { return layout_; }
        const vertex_buffer_layout& get_layout() const { return layout_; }

    private:
        int get_uniform_location(const std::string& uniform_name) const;

        friend class renderer;
        shader(const video_ctrl::renderer &rend, const char* fragment_code, const char* vertex_code);

        void unload() noexcept;
        void compile(uint32_t shader_id);
        void link();
        void cache_uniform_locations();

        vertex_buffer_layout layout_;
        std::unordered_map<std::string, int> locations_;

        uint32_t program_id_ = 0;
        uint32_t fragment_shader_id_ = 0;
        uint32_t vertex_shader_id_ = 0;
        mutable int32_t max_bound_slot_ = -1;

        const video_ctrl::renderer &rend_;
    };

    using shader_ptr = std::shared_ptr<shader>;
}
