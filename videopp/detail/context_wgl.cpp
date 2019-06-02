#include "context.h"
namespace video_ctrl
{
namespace
{
HGLRC shared_ctx{};
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT{};
}

context_wgl::context_wgl(void* native_handle)
{
	hwnd_ = reinterpret_cast<HWND>(native_handle);
	hdc_ = GetDC(hwnd_);

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

	int pixelFormat = ChoosePixelFormat(hdc_, &pfd);
	if(pixelFormat == 0)
	{
		throw video_ctrl::exception("Cannot ChoosePixelFormat.");
	}
	DescribePixelFormat(hdc_, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

	int result = SetPixelFormat(hdc_, pixelFormat, &pfd);
	if(result == 0)
	{
		throw video_ctrl::exception("Cannot SetPixelFormat.");
	}
	ctx_ = wglCreateContext(hdc_);
    if(shared_ctx)
    {
        wglShareLists(shared_ctx, ctx_);
    }
    else
    {
        shared_ctx = ctx_;
    }
	make_current();

	// must be called after context was made current
	load_extensions();
}

context_wgl::~context_wgl()
{
    if(shared_ctx == ctx_)
    {
        shared_ctx = {};
    }
	wglMakeCurrent(nullptr, nullptr);
	wglDeleteContext(ctx_);
	ReleaseDC(hwnd_, hdc_);
}

void context_wgl::load_extensions()
{
	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
}

bool context_wgl::set_vsync(bool vsync)
{
	if(wglSwapIntervalEXT)
	{
		return wglSwapIntervalEXT(vsync ? 1 : 0);
	}
	return false;
}

// make it the calling thread's current rendering context
bool context_wgl::make_current()
{
	return wglMakeCurrent(hdc_, ctx_);
}

bool context_wgl::swap_buffers()
{
	return SwapBuffers(hdc_);
}
}
