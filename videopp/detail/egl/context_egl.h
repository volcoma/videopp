#pragma once

#include "../../renderer.h"

#ifndef EGL_CAST
/* C++ / C typecast macros for special EGL handle values */
#if defined(__cplusplus)
#define EGL_CAST(type, value) (static_cast<type>(value))
#else
#define EGL_CAST(type, value) ((type) (value))
#endif
#endif

#include "glad_egl.h"

namespace video_ctrl
{
struct context_egl : context
{
    context_egl(void* native_handle, void* native_display);
    ~context_egl() override;

    bool set_vsync(bool vsync) override;
    bool make_current() override;
    bool swap_buffers() override;

    EGLSurface surface_{};
    EGLDisplay display_{};
    EGLContext context_{};

};


}
