#include "context_wgl.h"

namespace video_ctrl
{
namespace
{
context_wgl* master_context{};
}

context_wgl::context_wgl(void* native_handle)
{
    hwnd_ = reinterpret_cast<HWND>(native_handle);
    if(!hwnd_)
    {
        throw video_ctrl::exception("Invalid native handle.");
    }

	hdc_ = GetDC(hwnd_);
    if(!hdc_)
    {
        throw video_ctrl::exception("Could not get device context for native handle.");
    }

	PIXELFORMATDESCRIPTOR pfd;
	std::memset(&pfd, 0, sizeof(pfd));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pf = ChoosePixelFormat(hdc_, &pfd);
	if(pf == 0)
	{
		throw video_ctrl::exception("ChoosePixelFormat failed.");
	}
	if(!DescribePixelFormat(hdc_, pf, sizeof(pfd), &pfd))
    {
        throw video_ctrl::exception("DescribePixelFormat failed.");
    }

	if(!SetPixelFormat(hdc_, pf, &pfd))
	{
		throw video_ctrl::exception("SetPixelFormat failed.");
	}
	context_ = wglCreateContext(hdc_);

    if(!master_context)
    {
        master_context = this;
    }
    else
    {
        if(!wglShareLists(master_context->context_, context_))
        {
            throw video_ctrl::exception("wglShareLists failed.");
        }
    }


    make_current();

	// must be called after context was made current
    if(!gladLoadWGL(hdc_))
    {
        throw video_ctrl::exception("Cannot load wgl.");
    }
}

context_wgl::~context_wgl()
{
    if(master_context == this)
    {
        master_context = {};
    }
	wglMakeCurrent(nullptr, nullptr);
	wglDeleteContext(context_);
	ReleaseDC(hwnd_, hdc_);

    if(master_context)
    {
        master_context->make_current();
    }
}

bool context_wgl::set_vsync(bool vsync)
{
	if(wglSwapIntervalEXT)
	{
		return wglSwapIntervalEXT(vsync ? 1 : 0);
	}
	return false;
}

bool context_wgl::make_current()
{
	return wglMakeCurrent(hdc_, context_);
}

bool context_wgl::swap_buffers()
{
	return SwapBuffers(hdc_);
}
}
