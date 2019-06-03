#pragma once

#include "../renderer.h"

namespace video_ctrl
{
struct context_wgl : context
{
    context_wgl(void* native_handle);
    ~context_wgl() override;

    bool set_vsync(bool vsync) override;
    bool make_current() override;
    bool swap_buffers() override;

    void* hwnd_;
    void* hdc_{};
    void* ctx_{};
};


}
