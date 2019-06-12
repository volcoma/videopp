#pragma once

#include "../../renderer.h"
#include "glad_wgl.h"

namespace video_ctrl
{
struct context_wgl : context
{
    context_wgl(void* native_handle);
    ~context_wgl() override;

    bool set_vsync(bool vsync) override;
    bool make_current() override;
    bool swap_buffers() override;

    HWND hwnd_;
    HDC hdc_{};
    HGLRC context_{};
};


}
