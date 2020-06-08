#pragma once

#include "../../context.h"

#ifndef EGL_CAST
/* C++ / C typecast macros for special EGL handle values */
#if defined(__cplusplus)
#define EGL_CAST(type, value) (static_cast<type>(value))
#else
#define EGL_CAST(type, value) ((type) (value))
#endif
#endif

#include "glad_egl.h"

namespace gfx
{
struct context_egl : context
{
    context_egl(native_handle handle, native_display display, int major = 2, int minor = 0);
    ~context_egl() override;

    bool set_vsync(bool vsync) override;
    bool make_current() override;
    bool swap_buffers() override;

    //pixmap impl
    pixmap create_pixmap(const size& sz, pix_type pix_t) override;
    bool destroy_pixmap(const pixmap& p) const override;
    bool bind_pixmap(const pixmap& p) const override;
    void unbind_pixmap(const pixmap& p) const override;
    void* get_native_handle(const pixmap& p) const override;

    EGLSurface surface_{};
    EGLDisplay display_{};
    EGLContext context_{};

};


}
