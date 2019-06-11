#include "context_egl.h"

namespace video_ctrl
{
namespace
{
EGLContext shared_ctx{};
}

context_egl::context_egl(void* native_handle, void* native_display)
{
    display_ = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(native_display));
    if (display_ == EGL_NO_DISPLAY)
    {
        throw std::runtime_error("Cannot get EGL Dsiplay.");
    }

    if(!gladLoadEGL())
    {
        throw video_ctrl::exception("Cannot load glx.");
    }

    int major_version {};
    int minor_version {};

    if (!eglInitialize(display_, &major_version, &minor_version))
    {
        throw std::runtime_error("Cannot get EGL Initialize.");
    }

    EGLConfig config {};

    {
        EGLint num_config {};
        EGLint attribList[] =
            {
                EGL_RED_SIZE,       5,
                EGL_GREEN_SIZE,     6,
                EGL_BLUE_SIZE,      5,
                EGL_ALPHA_SIZE,     8,
                EGL_DEPTH_SIZE,     8,
                EGL_STENCIL_SIZE,   8,
                EGL_SAMPLE_BUFFERS, 1,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_NONE
            };

        // Choose config
        if (!eglChooseConfig(display_, attribList, &config, 1, &num_config))
        {
            throw std::runtime_error("Cannot choose EGL Config.");
        }

        if (num_config < 1)
        {
            throw std::runtime_error("No EGL Config.");
        }
    }

    auto native_win_handle = reinterpret_cast<uintptr_t>(native_handle);
    auto egl_native_window = reinterpret_cast<EGLNativeWindowType>(native_win_handle);

    surface_ = eglCreateWindowSurface(display_, config, egl_native_window, nullptr);

    if (surface_ == EGL_NO_SURFACE)
    {
        throw std::runtime_error("Cannot create EGL Surface.");
    }

    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    context_ = eglCreateContext(display_, config, shared_ctx, context_attribs);

    if (context_ == EGL_NO_CONTEXT)
    {
        throw std::runtime_error("Cannot create EGL Context.");
    }

    if(!shared_ctx)
    {
        shared_ctx = context_;
    }

    make_current();
}

context_egl::~context_egl()
{
    if(shared_ctx == context_)
    {
        shared_ctx = {};
    }

    eglMakeCurrent(display_, nullptr, nullptr, nullptr);
    eglDestroyContext(display_, context_);
}

bool context_egl::set_vsync(bool vsync)
{
    return eglSwapInterval(display_, vsync ? 1 : 0);
}

// make it the calling thread's current rendering context
bool context_egl::make_current()
{
    return eglMakeCurrent(display_, surface_, surface_, context_);
}

bool context_egl::swap_buffers()
{
    return eglSwapBuffers(display_, surface_);
}
}
