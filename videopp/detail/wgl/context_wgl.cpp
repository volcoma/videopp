#include "context_wgl.h"

namespace gfx
{
namespace
{
std::vector<context_wgl*> ctxs {};

bool make_current_context(context_wgl* ctx)
{
    static context_wgl* current{};

    if(current != ctx)
    {
        if(wglMakeCurrent(ctx->hdc_, ctx->context_))
        {
            current = ctx;
            return true;
        }

        return false;
    }

    return true;
}
}

context_wgl::context_wgl(void* native_handle, int major, int minor)
{
    hwnd_ = reinterpret_cast<HWND>(native_handle);
    if(!hwnd_)
    {
        throw gfx::exception("Invalid native handle.");
    }

	hdc_ = GetDC(hwnd_);
    if(!hdc_)
    {
        throw gfx::exception("Could not get device context for native handle.");
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
		throw gfx::exception("ChoosePixelFormat failed.");
	}
	if(!DescribePixelFormat(hdc_, pf, sizeof(pfd), &pfd))
    {
        throw gfx::exception("DescribePixelFormat failed.");
    }

	if(!SetPixelFormat(hdc_, pf, &pfd))
	{
		throw gfx::exception("SetPixelFormat failed.");
	}

    auto dummy_ctx = wglCreateContext(hdc_);
    wglMakeCurrent(hdc_, dummy_ctx);

    // must be called after context was made current
    if(!gladLoadWGL(hdc_))
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(dummy_ctx);
        throw gfx::exception("Cannot load wgl.");
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(dummy_ctx);

    int context_attribs[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, major,
		WGL_CONTEXT_MINOR_VERSION_ARB, minor,
		0
	};

    if(!ctxs.empty())
    {
        context_ = wglCreateContextAttribsARB(hdc_, ctxs.front()->context_, context_attribs);
    }
    else
    {
        context_ = wglCreateContextAttribsARB(hdc_, nullptr, context_attribs);
    }

    if(!context_)
    {
        throw gfx::exception("Cannot create wgl context.");
    }

    ctxs.emplace_back(this);

    make_current_context(this);
}

context_wgl::~context_wgl()
{
    auto it = std::remove(std::begin(ctxs), std::end(ctxs), this);
    ctxs.erase(it, std::end(ctxs));

    wglMakeCurrent(nullptr, nullptr);
	wglDeleteContext(context_);
	ReleaseDC(hwnd_, hdc_);

    if (!ctxs.empty())
    {
        ctxs.front()->make_current();
    }
}

bool context_wgl::set_vsync(bool vsync)
{
    bool result = false;
	if(wglSwapIntervalEXT)
	{
		result = wglSwapIntervalEXT(vsync ? -1 : 0);
        if(!result)
        {
            result = wglSwapIntervalEXT(vsync ? 1 : 0);
        }
	}
	return result;
}

bool context_wgl::make_current()
{
    return make_current_context(this);
}

bool context_wgl::swap_buffers()
{
    return SwapBuffers(hdc_);
}


}
