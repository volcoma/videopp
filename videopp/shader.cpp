#include "shader.h"
#include "renderer.h"
#include "detail/utils.h"

namespace video_ctrl
{
    namespace
    {
        void get_log(uint32_t id)
        {
            GLint info_len = 0;
            gl_call(glGetShaderiv(id, GL_INFO_LOG_LENGTH, &info_len));

            if (info_len > 1)
            {
                std::vector<char> shader_log(size_t(info_len), 0);
                gl_call(glGetShaderInfoLog(id, GLsizei(shader_log.size()), nullptr, shader_log.data()));

                log(std::string("errors linking : ") + shader_log.data());
            }
            else
            {
                log("unknown errors linking");
            }
        }
    }

    shader::shader(const renderer &rend, const char *fragment_code, const char *vertex_code)
        : rend_(rend)
    {
        // Create Shader And Program Objects
        program_id_ = glCreateProgram();
        vertex_shader_id_ = glCreateShader(GL_VERTEX_SHADER);
        fragment_shader_id_ = glCreateShader(GL_FRAGMENT_SHADER);

        if(fragment_code != nullptr && fragment_code != nullptr)
        {
            gl_call(glShaderSource(vertex_shader_id_, 1, &vertex_code, nullptr));
            gl_call(glShaderSource(fragment_shader_id_, 1, &fragment_code, nullptr));
        }

        // Compile The Shaders
        compile(vertex_shader_id_);
        compile(fragment_shader_id_);
        link();

        cache_uniform_locations();

        layout_.set_program_id(program_id_);
    }

    shader::~shader() noexcept
    {
        unload();
    }

    void shader::unload() noexcept
    {
        if (vertex_shader_id_ > 0)
        {
            gl_call(glDeleteShader(vertex_shader_id_));
            vertex_shader_id_ = 0;
        }

        if (fragment_shader_id_ > 0)
        {
            gl_call(glDeleteShader(fragment_shader_id_));
            fragment_shader_id_ = 0;
        }

        if (program_id_ > 0)
        {
            gl_call(glDeleteProgram(program_id_));
            program_id_ = 0;
        }
    }

    void shader::compile(uint32_t shader_id)
    {
        gl_call(glCompileShader(shader_id));

        GLint compiled{};
        gl_call(glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled));
        if (!compiled)
        {
            get_log(shader_id);
            unload();
            throw std::runtime_error("Cannot compile shader!");
        }
    }

    void shader::link()
    {
        // Attach The Shader Objects To The Program Object
        gl_call(glAttachShader(program_id_, vertex_shader_id_));
        gl_call(glAttachShader(program_id_, fragment_shader_id_));

        // Link The Program Object
        gl_call(glLinkProgram(program_id_));

        GLint linked {};
        gl_call(glGetProgramiv(program_id_, GL_LINK_STATUS, &linked));

        if (linked == GL_FALSE)
        {
            get_log(program_id_);
            unload();
            throw std::runtime_error("Cannot compile shader program.");
        }
    }

    void shader::cache_uniform_locations()
    {
        GLint uniforms{};
        gl_call(glGetProgramiv(program_id_, GL_ACTIVE_UNIFORMS, &uniforms));
        GLint max_len{};
        gl_call(glGetProgramiv(program_id_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_len));
        std::vector<GLchar> name_data(static_cast<size_t>(max_len));
        for(GLint idx = 0; idx < uniforms; ++idx)
        {
            GLint sz{};
            GLenum type{};
            GLsizei len{};
            gl_call(glGetActiveUniform(program_id_, GLuint(idx), GLsizei(name_data.size()), &len, &sz, &type, name_data.data()));
            std::string uniform_name(name_data.data(), static_cast<size_t>(len));

            auto location = glGetUniformLocation(program_id_, uniform_name.c_str());
            if(location >= 0)
            {
                locations_[uniform_name] = location;
            }
        }
    }

    void shader::enable() const
    {
        gl_call(glUseProgram(program_id_));

        layout_.bind();
    }

    void shader::disable() const
    {
        layout_.unbind();

        gl_call(glUseProgram(0));
        clear_textures();
    }

    void shader::set_uniform(const std::string& uniform_name, const math::transform_t<float>::mat4_t &data) const
    {
        auto location = get_uniform_location(uniform_name);
        if(location >= 0)
        {
            gl_call(glUniformMatrix4fv(location, 1, false, math::value_ptr(data)));
        }
    }

    void shader::set_uniform(const std::string& uniform_name, const texture_ptr& tex, uint32_t slot,
                             texture::wrap_type wrap_type, texture::interpolation_type interp_type) const
    {
        set_uniform(uniform_name, texture_view(tex), slot, wrap_type, interp_type);
    }

    void shader::set_uniform(const std::string& uniform_name, const texture_view& tex, uint32_t slot,
                             texture::wrap_type wrap_type, texture::interpolation_type interp_type) const
    {
        max_bound_slot_ = std::max(max_bound_slot_, int32_t(slot));
        rend_.set_texture(tex, slot, wrap_type, interp_type);
        auto location = get_uniform_location(uniform_name);
        if(location >= 0)
        {
            gl_call(glUniform1i(location, GLint(slot)));
        }
    }

    void shader::set_uniform(const std::string& uniform_name, const math::transform_t<int>::vec2_t& data) const
    {
        auto location = get_uniform_location(uniform_name);
        if(location >= 0)
        {
            gl_call(glUniform2i(location, data.x, data.y));
        }
    }

    void shader::set_uniform(const std::string& uniform_name, const math::transform_t<float>::vec2_t& data) const
    {
        auto location = get_uniform_location(uniform_name);
        if(location >= 0)
        {
            gl_call(glUniform2f(location, data.x, data.y));
        }
    }

    void shader::set_uniform(const std::string& uniform_name, const math::transform_t<int>::vec3_t& data) const
    {
        auto location = get_uniform_location(uniform_name);
        if(location >= 0)
        {
            gl_call(glUniform3i(location, data.x, data.y, data.z));
        }
    }

    void shader::set_uniform(const std::string& uniform_name, const math::transform_t<float>::vec3_t& data) const
    {
        auto location = get_uniform_location(uniform_name);
        if(location >= 0)
        {
            gl_call(glUniform3f(location, data.x, data.y, data.z));
        }
    }

    void shader::set_uniform(const std::string& uniform_name, const math::transform_t<int>::vec4_t& data) const
    {
        auto location = get_uniform_location(uniform_name);
        if(location >= 0)
        {
            gl_call(glUniform4i(location, data.x, data.y, data.z, data.w));
        }
    }

    void shader::set_uniform(const std::string& uniform_name, const math::transform_t<float>::vec4_t& data) const
    {
        auto location = get_uniform_location(uniform_name);
        if(location >= 0)
        {
            gl_call(glUniform4f(location, data.x, data.y, data.z, data.w));
        }
    }

    void shader::set_uniform(const std::string& uniform_name, const color& data) const
    {
        set_uniform(uniform_name, math::vec4{data.r / 255.0f, data.g / 255.0f, data.b / 255.0f, data.a / 255.0f});
    }

    bool shader::has_uniform(const std::string& uniform_name) const
    {
        return locations_.find(uniform_name) != std::end(locations_);
    }

    void shader::clear_textures() const
    {
        for(int32_t slot = 0; slot <= max_bound_slot_; ++slot)
        {
            rend_.set_texture(0, uint32_t(slot));
        }
        max_bound_slot_ = -1;
    }

    int shader::get_uniform_location(const std::string& uniform_name) const
    {
        auto it = locations_.find(uniform_name);
        if(it != std::end(locations_))
        {
            return it->second;
        }

        log("ERROR, could not find uniform: " + uniform_name);

        return -1;
    }

    void shader::set_uniform(const std::string& uniform_name, float data) const
    {
        auto location = get_uniform_location(uniform_name);
        if(location >= 0)
        {
            gl_call(glUniform1f(location, data));
        }
    }

    void shader::set_uniform(const std::string& uniform_name, int data) const
    {
        auto location = get_uniform_location(uniform_name);
        if(location >= 0)
        {
            gl_call(glUniform1i(location, data));
        }
    }
}
