#pragma once

#include "pixel_type.h"
#include "size.h"

namespace gfx
{
    using native_handle = void*;
    using native_display = void*;
    using pixmap = uint64_t;

    constexpr pixmap pixmap_invalid_id {};

    struct context
    {
        virtual ~context() = default;
        virtual bool make_current() = 0;
        virtual bool swap_buffers() = 0;
        virtual bool set_vsync(bool vsync) = 0;

        //pixmap impl
        virtual pixmap create_pixmap(const size&, pix_type) = 0;
        virtual bool destroy_pixmap(const pixmap& p) const = 0;
        virtual bool bind_pixmap(const pixmap&) const = 0;
        virtual void unbind_pixmap(const pixmap&) const = 0;
        virtual void* get_native_handle(const pixmap& p) const = 0;
    };
}
