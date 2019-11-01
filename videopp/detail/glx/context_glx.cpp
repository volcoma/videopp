#include "context_glx.h"

namespace gfx
{
namespace
{
context_glx* master_ctx{};
}


context_glx::context_glx(void* native_handle, void* native_display)
{
    surface_ = reinterpret_cast<Window>(reinterpret_cast<uintptr_t>(native_handle));
    display_ = reinterpret_cast<Display*>(native_display);

    if(!gladLoadGLX(display_, 0))
    {
        throw gfx::exception("Cannot load glx.");
    }

    static int visual_attribs[] = {
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_DOUBLEBUFFER, true,
            GLX_RED_SIZE, 1,
            GLX_GREEN_SIZE, 1,
            GLX_BLUE_SIZE, 1,
            None
        };

    int num_fbc = 0;
    GLXFBConfig *fbc = glXChooseFBConfig(display_,
                                         DefaultScreen(display_),
                                         visual_attribs, &num_fbc);
    if (!fbc)
    {
        throw gfx::exception("glXChooseFBConfig() failed");
    }

    XVisualInfo* vi = glXGetVisualFromFBConfig(display_, fbc[0]);
    if(master_ctx)
    {
        context_ = glXCreateContext(display_, vi, master_ctx->context_, GL_TRUE);
    }
    else
    {
        context_ = glXCreateContext(display_, vi, nullptr, GL_TRUE);
    }
    if (!context_)
    {
        throw gfx::exception("Failed to create OpenGL context");
    }

    if(!master_ctx)
    {
        master_ctx = this;
    }


    make_current();
}

context_glx::~context_glx()
{
    if(master_ctx == this)
    {
        master_ctx = {};
    }

    glXMakeCurrent(display_, 0, nullptr);
    glXDestroyContext(display_, context_);

    if(master_ctx)
    {
        master_ctx->make_current();
    }
}

bool context_glx::set_vsync(bool vsync)
{
    if(glXSwapIntervalEXT)
    {
        glXSwapIntervalEXT(display_, surface_, vsync ? 1 : 0);
    }
    else if(glXSwapIntervalMESA)
    {
        glXSwapIntervalMESA(vsync ? 1 : 0);
    }
    else if(glXSwapIntervalSGI)
    {
        glXSwapIntervalSGI(vsync ? 1 : 0);
    }
	return true;
}

// make it the calling thread's current rendering context
bool context_glx::make_current()
{
    return glXMakeCurrent(display_, surface_, context_);
}

bool context_glx::swap_buffers()
{
    glXSwapBuffers(display_, surface_);
    return true;
}
}
