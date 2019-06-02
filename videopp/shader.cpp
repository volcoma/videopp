#include "shader.h"
#include "renderer.h"
#include "utils.h"

namespace video_ctrl
{

    shader::shader(const renderer &rend, const char *fragment_code, const char *vertex_code)
        : rend_(rend)
    {
        // Create Shader And Program Objects
        program_id_ = glCreateProgram();
        vertex_shader_id_ = glCreateShader(GL_VERTEX_SHADER);
        fragment_shader_id_ = glCreateShader(GL_FRAGMENT_SHADER);

        if(fragment_code != nullptr && fragment_code != nullptr)
        {
            gl_call(glShaderSource(vertex_shader_id_, 1, reinterpret_cast<const GLchar**> (&vertex_code), nullptr));
            gl_call(glShaderSource(fragment_shader_id_, 1, reinterpret_cast<const GLchar**> (&fragment_code), nullptr));
        }

        // Compile The Shaders
        compile_shader(vertex_shader_id_);
        compile_shader(fragment_shader_id_);

        // Attach The Shader Objects To The Program Object
        gl_call(glAttachShader(program_id_, vertex_shader_id_));
        gl_call(glAttachShader(program_id_, fragment_shader_id_));

        // Link The Program Object
        GLint linked {};

        gl_call(glLinkProgram(program_id_));
        gl_call(glGetProgramiv(program_id_, GL_LINK_STATUS, &linked));

        if (linked == GL_FALSE)
        {
            get_log(program_id_);
            auto err = glGetError();
            if (err != GL_NO_ERROR)
            {
                log("ERROR IN SHADER! glGetError reported error " + std::to_string(err));
            }

            unload();
            throw std::runtime_error("Cannot compile shader program.");
        }
    }

    shader::~shader() noexcept
    {
        unload();
    }

    void shader::unload() noexcept
    {
        delete_object(vertex_shader_id_);
        delete_object(fragment_shader_id_);

        if (program_id_ > 0)
        {
            gl_call(glDeleteProgram(program_id_));
            program_id_ = 0;
        }
    }

    void shader::delete_object(uint32_t &obj) noexcept
    {
        if (obj > 0)
        {
            gl_call(glDeleteShader(obj));
            obj = 0;
        }
    }

    void shader::get_log(uint32_t id)
    {
        GLint info_len = 0;
        gl_call(glGetShaderiv(id, GL_INFO_LOG_LENGTH, &info_len));

        if (info_len > 1)
        {
            std::vector<char> shader_log(info_len, 0);
            gl_call(glGetShaderInfoLog(id, shader_log.size(), nullptr, shader_log.data()));

            log(std::string("errors linking : ") + shader_log.data());
        }
        else
        {
            log("unknown errors linking");
        }
    }

    void shader::compile_shader(uint32_t shader_id)
    {
        GLint compiled;
        gl_call(glCompileShader(shader_id));
        gl_call(glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled));
        if (!compiled)
        {
            get_log(shader_id);
            unload();
            throw std::runtime_error("Cannot compile shader!");
        }
    }

    void shader::enable() const
    {
        gl_call(glUseProgram(program_id_));
    }

    void shader::disable() const
    {
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
        return get_uniform_location(uniform_name) >= 0;

    }

    void shader::clear_textures() const
    {
        for(int32_t slot = 0; slot <= max_bound_slot_; ++slot)
        {
            rend_.set_texture(0, slot);
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

        auto location = glGetUniformLocation(program_id_, uniform_name.c_str());
        if(location >= 0)
        {
            locations_[uniform_name] = location;
        }
        else
        {
            log("ERROR, could not find uniform: " + uniform_name);
        }

        return location;
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
