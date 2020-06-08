#pragma once

#include "../../renderer.h"
#include "glad_wgl.h"

namespace gfx
{
struct context_wgl : context
{
    context_wgl(void* native_handle, int major = 3, int minor = 0);
    ~context_wgl() override;

    bool set_vsync(bool vsync) override;
    bool make_current() override;
    bool swap_buffers() override;

    pixmap create_pixmap(const size&, pix_type) override { throw std::runtime_error("Not implemented!"); }
    bool destroy_pixmap(const pixmap& p) const override { return false; }
    bool bind_pixmap(const pixmap&) const override { return false; }
    void unbind_pixmap(const pixmap&) const override{ }
    void* get_native_handle(const pixmap& p) const override { return nullptr; }

    HWND hwnd_;
    HDC hdc_{};
    HGLRC context_{};
};


}
