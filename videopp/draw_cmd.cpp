#include "draw_cmd.h"

namespace gfx
{


gpu_program& simple_program() noexcept
{
    static gpu_program program;
    return program;
}

gpu_program& multi_channel_texture_program() noexcept
{
    static gpu_program program;
    return program;
}

gpu_program& multi_channel_dither_texture_program() noexcept
{
    static gpu_program program;
    return program;
}

gpu_program& single_channel_texture_program() noexcept
{
    static gpu_program program;
    return program;
}

gpu_program& distance_field_font_program() noexcept
{
    static gpu_program program;
    return program;
}

gpu_program& blur_program() noexcept
{
    static gpu_program program;
    return program;
}

gpu_program& fxaa_program() noexcept
{
    static gpu_program program;
    return program;
}

font_ptr& default_font() noexcept
{
    static font_ptr font;
    return font;
}

}
