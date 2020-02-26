#include "context_glx.h"

namespace gfx
{
namespace
{

bool ctx_error = false;
int ctx_error_handler(Display* /*dpy*/, XErrorEvent* /*ev*/)
{
	ctx_error = true;
	return 0;
}
context_glx* master_ctx{};
}


context_glx::context_glx(void* native_handle, void* native_display, int major, int minor)
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
		throw gfx::exception("glXChooseFBConfig() failed. Failed to retrieve a framebuffer config");
    }

	printf( "Found %d matching FB configs.\n", num_fbc );

	// Pick the FB config/visual with the most samples per pixel
	printf( "Getting XVisualInfos\n" );
	int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;

	for (int i=0; i< num_fbc; ++i)
	{
		XVisualInfo *vi = glXGetVisualFromFBConfig( display_, fbc[i] );
		if ( vi )
		{
			int samp_buf, samples;
			glXGetFBConfigAttrib( display_, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf );
			glXGetFBConfigAttrib( display_, fbc[i], GLX_SAMPLES       , &samples  );

			printf( "  Matching fbconfig %d, visual ID 0x%2x: SAMPLE_BUFFERS = %d,"
				   " SAMPLES = %d\n",
				   i, unsigned(vi -> visualid), samp_buf, samples );

			if ( best_fbc < 0 || (samp_buf && samples > best_num_samp) )
				best_fbc = i, best_num_samp = samples;
			if ( worst_fbc < 0 || (!samp_buf || samples < worst_num_samp) )
				worst_fbc = i, worst_num_samp = samples;
		}
		XFree( vi );
	}

	GLXFBConfig bestFbc = fbc[ best_fbc ];

	// Be sure to free the FBConfig list allocated by glXChooseFBConfig()
	XFree( fbc );


	int context_attribs[] =
	{
		GLX_CONTEXT_MAJOR_VERSION_ARB, major,
		GLX_CONTEXT_MINOR_VERSION_ARB, minor,
		//GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		None
	};

	// Couldn't create GL 3.0 context.  Fall back to old-style 2.x context.
	// When a context version below 3.0 is requested, implementations will
	// return the newest context version compatible with OpenGL versions less
	// than version 3.0.
	// GLX_CONTEXT_MAJOR_VERSION_ARB = 1

	int (*oldHandler)(Display*, XErrorEvent*) =
		XSetErrorHandler(&ctx_error_handler);

	ctx_error = false;
    //XVisualInfo* vi = glXGetVisualFromFBConfig(display_, fbc[0]);
    if(master_ctx)
    {
		context_ = glXCreateContextAttribsARB(display_, bestFbc, master_ctx->context_, True, context_attribs);
    }
    else
    {
		context_ = glXCreateContextAttribsARB(display_, bestFbc, nullptr, True, context_attribs);
    }
	if (ctx_error || !context_)
	{
		throw gfx::exception("Failed to create OpenGL GLX context " + std::to_string(major) + "." + std::to_string(minor));
    }


	// Sync to ensure any errors generated are processed.
	XSync( display_, False );

	// Restore the original error handler
	XSetErrorHandler( oldHandler );

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
