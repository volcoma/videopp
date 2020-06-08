#pragma once

#include "../../context.h"
#include "glad_glx.h"

#include <map>

namespace gfx
{
struct pixmap_glx
{
    uint64_t xpixmap = 0;
    uint64_t glx_pixmap = 0;
};

struct context_glx : context
{
    context_glx(native_handle handle, native_display display, int major = 1, int minor = 0);
    ~context_glx() override;

    bool set_vsync(bool vsync) override;
    bool make_current() override;
    bool swap_buffers() override;
    //pixmap impl
    pixmap create_pixmap(const size& sz, pix_type pix_t) override;
    bool destroy_pixmap(const pixmap& p) const override;
    bool bind_pixmap(const pixmap& p) const override;
    void unbind_pixmap(const pixmap& p) const override;
    void* get_native_handle(const pixmap& p) const override;

    Window surface_{};
    ::Display* display_{};
    GLXContext context_{};
};


}
