#include "context_egl.h"

namespace gfx
{
namespace
{
context_egl* master_ctx{};
}

context_egl::context_egl(void* native_handle, void* native_display, int major, int minor)
{
    display_ = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(native_display));
    if (display_ == EGL_NO_DISPLAY)
    {
        throw gfx::exception("Cannot get EGL Dsiplay.");
    }

    if(!gladLoadEGL())
    {
        throw gfx::exception("Cannot load glx.");
    }

    int major_version {};
    int minor_version {};

    if (!eglInitialize(display_, &major_version, &minor_version))
    {
        throw gfx::exception("Cannot get EGL Initialize.");
    }

    EGLConfig config {};

    {
        EGLint num_config {};
        EGLint attribList[] =
            {
                EGL_RENDERABLE_TYPE,
                EGL_OPENGL_ES2_BIT,
                EGL_NONE
            };

        // Choose config
        if (!eglChooseConfig(display_, attribList, &config, 1, &num_config))
        {
            throw gfx::exception("Cannot choose EGL Config.");
        }

        if (num_config < 1)
        {
            throw gfx::exception("No EGL Config.");
        }
    }

    auto native_win_handle = reinterpret_cast<uintptr_t>(native_handle);
    auto egl_native_window = reinterpret_cast<EGLNativeWindowType>(native_win_handle);

    surface_ = eglCreateWindowSurface(display_, config, egl_native_window, nullptr);

    if (surface_ == EGL_NO_SURFACE)
    {
        throw gfx::exception("Cannot create EGL Surface.");
    }

	EGLint context_attribs[] = { EGL_CONTEXT_MAJOR_VERSION, major, EGL_CONTEXT_MINOR_VERSION, minor, EGL_NONE };
    if(master_ctx)
    {
        context_ = eglCreateContext(display_, config, master_ctx->context_, context_attribs);
    }
    else
    {
        context_ = eglCreateContext(display_, config, nullptr, context_attribs);
    }

    if (context_ == EGL_NO_CONTEXT)
    {
		throw gfx::exception("Failed to create OpenGL ES context " + std::to_string(major) + "." + std::to_string(minor));
	}

    if(!master_ctx)
    {
        master_ctx = this;
    }

    make_current();
}

context_egl::~context_egl()
{
    if(master_ctx == this)
    {
        master_ctx = {};
    }

    eglMakeCurrent(display_, nullptr, nullptr, nullptr);
    eglDestroyContext(display_, context_);

    if(master_ctx)
    {
        master_ctx->make_current();
    }
}

bool context_egl::set_vsync(bool vsync)
{
    return eglSwapInterval(display_, vsync ? 1 : 0);
}

bool context_egl::make_current()
{
    return eglMakeCurrent(display_, surface_, surface_, context_);
}

bool context_egl::swap_buffers()
{
    return eglSwapBuffers(display_, surface_);
}
}
