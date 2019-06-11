#pragma once

#include "../renderer.h"
#include "../glad/glad_glx.h"

namespace video_ctrl
{
struct context_glx : context
{
    context_glx(void* native_handle, void* native_display);
    ~context_glx() override;

    bool set_vsync(bool vsync) override;
    bool make_current() override;
    bool swap_buffers() override;

    Window surface_{};
    ::Display* display_{};
    GLXContext context_{};
};


}
