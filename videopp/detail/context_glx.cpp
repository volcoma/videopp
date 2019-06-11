#include "context_glx.h"

namespace video_ctrl
{
namespace
{
GLXContext shared_ctx{};
}


context_glx::context_glx(void* native_handle, void* native_display)
{
    surface_ = reinterpret_cast<Window>(reinterpret_cast<uintptr_t>(native_handle));
    display_ = reinterpret_cast<Display*>(native_display);

    if(!gladLoadGLX(display_, 0))
    {
        throw video_ctrl::exception("Cannot load glx.");
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
        throw std::runtime_error("glXChooseFBConfig() failed");
    }

    XVisualInfo* vi = glXGetVisualFromFBConfig(display_, fbc[0]);
    context_ = glXCreateContext(display_, vi, shared_ctx, GL_TRUE);
    if (!context_)
    {
        throw std::runtime_error("Failed to create OpenGL context");
    }

    if(!shared_ctx)
    {
        shared_ctx = context_;
    }


    make_current();
}

context_glx::~context_glx()
{
    if(shared_ctx == context_)
    {
        shared_ctx = {};
    }

    glXMakeCurrent(display_, 0, nullptr);
    glXDestroyContext(display_, context_);
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
