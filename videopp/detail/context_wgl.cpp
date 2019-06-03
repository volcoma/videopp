#include "context_wgl.h"
#include "../glad/glad_wgl.h"

namespace video_ctrl
{
namespace
{
HGLRC shared_ctx{};

}

context_wgl::context_wgl(void* native_handle)
{
    auto hwnd = reinterpret_cast<HWND>(native_handle);
	hwnd_ = hwnd;

	auto hdc = GetDC(hwnd);
    hdc_ = hdc;


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

	int pixelFormat = ChoosePixelFormat(hdc, &pfd);
	if(pixelFormat == 0)
	{
		throw video_ctrl::exception("Cannot ChoosePixelFormat.");
	}
	DescribePixelFormat(hdc, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

	int result = SetPixelFormat(hdc, pixelFormat, &pfd);
	if(result == 0)
	{
		throw video_ctrl::exception("Cannot SetPixelFormat.");
	}
	auto ctx = wglCreateContext(hdc);
    ctx_ = ctx;

    if(shared_ctx)
    {
        wglShareLists(shared_ctx, ctx);
    }
    else
    {
        shared_ctx = ctx;
    }

    if(!wglMakeCurrent(hdc, ctx))
    {
        throw video_ctrl::exception("Cannot make_current.");
    }

	// must be called after context was made current
    if(!gladLoadWGL(hdc))
    {
        throw video_ctrl::exception("Cannot load wgl.");
    }

}

context_wgl::~context_wgl()
{
    auto ctx = reinterpret_cast<HGLRC>(ctx_);
    auto hdc = reinterpret_cast<HDC>(hdc_);
    auto hwnd = reinterpret_cast<HWND>(hwnd_);

    if(shared_ctx == ctx)
    {
        shared_ctx = {};
    }
	wglMakeCurrent(nullptr, nullptr);
	wglDeleteContext(ctx);
	ReleaseDC(hwnd, hdc);
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
    auto ctx = reinterpret_cast<HGLRC>(ctx_);
    auto hdc = reinterpret_cast<HDC>(hdc_);
	return wglMakeCurrent(hdc, ctx);
}

bool context_wgl::swap_buffers()
{
    auto hdc = reinterpret_cast<HDC>(hdc_);
	return SwapBuffers(hdc);
}
}
