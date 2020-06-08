#include "context_egl.h"

#include "../../utils.h"
#include "../../logger.h"
#include <algorithm>

namespace gfx
{
namespace
{
std::vector<context_egl*> ctxs {};

bool make_current_context(context_egl* ctx)
{
	static context_egl* current{};

    if(current != ctx)
    {
		if(eglMakeCurrent(ctx->display_, ctx->surface_, ctx->surface_, ctx->context_))
		{
			current = ctx;
			return true;
		}

        return false;
    }

    return true;
}
}

context_egl::context_egl(native_handle handle, native_display display, int major, int minor)
{
    display_ = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(display));
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
                EGL_OPENGL_ES3_BIT,
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

    auto native_win_handle = reinterpret_cast<uintptr_t>(handle);
    auto egl_native_window = reinterpret_cast<EGLNativeWindowType>(native_win_handle);

    surface_ = eglCreateWindowSurface(display_, config, egl_native_window, nullptr);

    if (surface_ == EGL_NO_SURFACE)
    {
        throw gfx::exception("Cannot create EGL Surface.");
    }

    EGLint context_attribs[] =
    {
        EGL_CONTEXT_MAJOR_VERSION, major,
        EGL_CONTEXT_MINOR_VERSION, minor,
//        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };

    if(!ctxs.empty())
    {
        context_ = eglCreateContext(display_, config, ctxs.front()->context_, context_attribs);
    }
    else
    {
        context_ = eglCreateContext(display_, config, nullptr, context_attribs);
    }

    if (context_ == EGL_NO_CONTEXT)
    {
        throw gfx::exception("Failed to create OpenGL ES context " + std::to_string(major) + "." + std::to_string(minor));
	}

    log("OpenGL ES " + std::to_string(major) + "." + std::to_string(minor) + " context was created.");

    ctxs.push_back(this);

    make_current_context(this);
}

context_egl::~context_egl()
{
    auto it = std::remove(std::begin(ctxs), std::end(ctxs), this);
    ctxs.erase(it, std::end(ctxs));

    eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display_, context_);

    if (!ctxs.empty())
    {
        ctxs.front()->make_current();
    }
}

bool context_egl::set_vsync(bool vsync)
{
    bool result = false;
    result = eglSwapInterval(display_, vsync ? -1 : 0);
    if(!result)
    {
        result = eglSwapInterval(display_, vsync ? 1 : 0);
    }

    return result;
}

bool context_egl::make_current()
{
    return make_current_context(this);
}

bool context_egl::swap_buffers()
{
	return eglSwapBuffers(display_, surface_);
}

pixmap context_egl::create_pixmap(const size&, pix_type)
{
    throw gfx::exception("Not implemented yet.");
}

void* context_egl::get_native_handle(const pixmap& p) const
{
    throw gfx::exception("Not implemented yet.");
}

bool context_egl::destroy_pixmap(const pixmap&) const
{
    throw gfx::exception("Not implemented yet.");
}

bool context_egl::bind_pixmap(const pixmap&) const
{
    return false;
}

void context_egl::unbind_pixmap(const pixmap&) const
{
}
}
