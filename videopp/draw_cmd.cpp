#include "draw_cmd.h"

namespace video_ctrl
{

texture_view::texture_view(const texture_ptr& texture) noexcept
{
    *this = create(texture);
}

texture_view::texture_view(uint32_t tex_id, uint32_t tex_width, uint32_t tex_height) noexcept
{
    *this = create(tex_id, tex_width, tex_height);
}

bool texture_view::operator==(const texture_view& rhs) const noexcept
{
    return id == rhs.id && width == rhs.width && height == rhs.height;
}

void* texture_view::get() const noexcept
{
    return reinterpret_cast<void*>(intptr_t(id));
}

texture_view texture_view::create(const texture_ptr& texture) noexcept
{
    texture_view view{};
    if(texture)
    {
        view.width = std::uint32_t(texture->get_rect().w);
        view.height = std::uint32_t(texture->get_rect().h);
        view.id = texture->get_id();
    }

    return view;
}

texture_view texture_view::create(uint32_t tex_id, uint32_t tex_width, uint32_t tex_height) noexcept
{
    texture_view view;
    view.id = tex_id;
    view.width = tex_width;
    view.height = tex_height;
    return view;
}

gpu_program& simple_program()
{
    static gpu_program program;
    return program;
}

gpu_program& multi_channel_texture_program()
{
    static gpu_program program;
    return program;
}

gpu_program& single_channel_texture_program()
{
    static gpu_program program;
    return program;
}

gpu_program& distance_field_font_program()
{
    static gpu_program program;
    return program;
}

gpu_program& blur_program()
{
    static gpu_program program;
    return program;
}

gpu_program& fxaa_program()
{
    static gpu_program program;
    return program;
}

font_ptr& default_font()
{
    static font_ptr font;
    return font;
}

}
