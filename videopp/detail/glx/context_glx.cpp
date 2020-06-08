#include "context_glx.h"

#include "../../utils.h"
#include "../../logger.h"
#include <algorithm>

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

int swap_interval = 0;
int x11_gl_get_swap_interval(Display* display, Window drawable)
{
    if (glXSwapIntervalEXT) {

        unsigned int allow_late_swap_tearing = 0;
        unsigned int interval = 0;

        if (GLAD_GLX_EXT_swap_control_tear)
        {
            glXQueryDrawable(display,
                             drawable,
                             GLX_LATE_SWAPS_TEAR_EXT,
                             &allow_late_swap_tearing);
        }

        glXQueryDrawable(display, drawable,GLX_SWAP_INTERVAL_EXT, &interval);

        if ((allow_late_swap_tearing) && (interval > 0))
        {
            return -(int(interval));
        }

        return int(interval);
    }
    else if (glXGetSwapIntervalMESA)
    {
        return glXGetSwapIntervalMESA();
    }
    else
    {
        return swap_interval;
    }
}

void check_glx_version(Display* display)
{
    int glx_major, glx_minor;
    // FBConfigs were added in GLX version 1.3.
    if (!glXQueryVersion(display, &glx_major, &glx_minor) ||
        ((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1))
    {
        throw gfx::exception("Invalid GLX version");
    }
}

template <typename T, std::size_t N>
constexpr std::size_t countof(T const (&)[N]) noexcept
{
    return N;
}

GLXFBConfig choose_fb_config(Display* display)
{
    check_glx_version(display);
    printf("Getting matching framebuffer configs\n");

    int visual_attribs[] =
    {

        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        0
    };


    // Find suitable config
    GLXFBConfig best_fbc = nullptr;

    int num_configs{};
    GLXFBConfig* configs = glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &num_configs);

    printf("glx num configs %d\n", num_configs);

    for (int ii = 0; ii < num_configs; ++ii)
    {
        auto visual = glXGetVisualFromFBConfig(display, configs[ii]);
        if (visual)
        {
            printf("---\n");
            bool valid = true;
            for (uint32_t attr = 6; attr < countof(visual_attribs)-1 && visual_attribs[attr] != 0; attr += 2)
            {
                int value;
                glXGetFBConfigAttrib(display, configs[ii], visual_attribs[attr], &value);
                printf("glX %d/%d %2d: %4x, %8x (%8x%s)\n"
                         , ii
                         , num_configs
                         , attr/2
                         , visual_attribs[attr]
                         , value
                         , visual_attribs[attr + 1]
                         , value < visual_attribs[attr + 1] ? " *" : ""
                         );

                if (value < visual_attribs[attr + 1])
                {
                    valid = false;
                    break;
                }
            }

            if (valid)
            {
                best_fbc = configs[ii];
                printf("Best config %d.\n", ii);
                break;
            }
        }

        XFree(visual);
    }

    XFree(configs);
    return best_fbc;
}

bool make_current_context(context_glx* ctx)
{
	static context_glx* current{};

    if(current != ctx)
    {
		if(glXMakeCurrent(ctx->display_, ctx->surface_, ctx->context_))
		{
			current = ctx;
			return true;
		}

        return false;
    }

    return true;
}

std::vector<context_glx*> ctxs {};
std::map<pixmap, pixmap_glx> pixmaps {};
pixmap unique_id {};

}

context_glx::context_glx(native_handle handle, native_display display, int major, int minor)
{
    surface_ = reinterpret_cast<Window>(reinterpret_cast<uintptr_t>(handle));
    display_ = reinterpret_cast<Display*>(display);

    if(!gladLoadGLX(display_, 0))
    {
        throw gfx::exception("Cannot load glx.");
    }

    auto best_fbc = choose_fb_config(display_);
    if(!best_fbc)
    {
        throw gfx::exception("Failed to find a suitable X11 display configuration.");
    }

    int32_t flags = 0;//debug ? GLX_CONTEXT_DEBUG_BIT_ARB : 0;
    int context_attribs[] =
    {
        GLX_CONTEXT_MAJOR_VERSION_ARB, major,
        GLX_CONTEXT_MINOR_VERSION_ARB, minor,
        GLX_CONTEXT_FLAGS_ARB, flags,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
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
    if(!ctxs.empty())
    {
        context_ = glXCreateContextAttribsARB(display_, best_fbc, ctxs.front()->context_, True, context_attribs);
    }
    else
    {
        context_ = glXCreateContextAttribsARB(display_, best_fbc, nullptr, True, context_attribs);
    }
    if (ctx_error || !context_)
    {
        throw gfx::exception("Failed to create OpenGL GLX context " + std::to_string(major) + "." + std::to_string(minor));
    }

    // Sync to ensure any errors generated are processed.
    XSync( display_, False );

    // Restore the original error handler
    XSetErrorHandler( oldHandler );

    log("OpenGL GLX " + std::to_string(major) + "." + std::to_string(minor) + " context was created.");

    ctxs.push_back(this);
    make_current_context(this);
}

context_glx::~context_glx()
{
    auto it = std::remove(std::begin(ctxs), std::end(ctxs), this);
    ctxs.erase(it, std::end(ctxs));

    glXMakeCurrent(display_, 0, nullptr);
    glXDestroyContext(display_, context_);

    if (!ctxs.empty())
    {
        ctxs.front()->make_current();
    }
}

bool context_glx::set_vsync(bool vsync)
{
    bool result = false;

    if(GLAD_GLX_EXT_swap_control_tear)
    {
        swap_interval = vsync ? -1 : 0;
    }
    else
    {
        swap_interval = vsync ? 1 : 0;
    }


    if(glXSwapIntervalEXT)
    {
        /*
         * This is a workaround for a bug in NVIDIA drivers. Bug has been reported
         * and will be fixed in a future release (probably 319.xx).
         *
         * There's a bug where glXSetSwapIntervalEXT ignores updates because
         * it has the wrong value cached. To work around it, we just run a no-op
         * update to the current value.
         */
        int current_interval = x11_gl_get_swap_interval(display_, surface_);
        glXSwapIntervalEXT(display_, surface_, current_interval);
        glXSwapIntervalEXT(display_, surface_, swap_interval);
    }
    else if(glXSwapIntervalMESA)
    {
        if(glXSwapIntervalMESA(swap_interval) != 0)
        {
            return false;
        }
    }
    else if(glXSwapIntervalSGI)
    {
        if(glXSwapIntervalSGI(vsync ? 1 : 0) != 0)
        {
            return false;
        }
    }
    return true;
}

bool context_glx::make_current()
{
	return make_current_context(this);
}

bool context_glx::swap_buffers()
{
    glXSwapBuffers(display_, surface_);
	return true;
}

pixmap context_glx::create_pixmap(const size& sz, pix_type pix_t)
{
    make_current();

    pixmap_glx p;

    p.xpixmap = XCreatePixmap(display_, surface_, static_cast<uint32_t> (sz.w), static_cast<uint32_t> (sz.h),
                              static_cast<uint32_t> (8 * bytes_per_pixel(pix_t)));

    if (p.xpixmap == 0)
    {
        throw gfx::exception("Cannot create xpixmap buffer");
    }

        std::vector<int> attributes {
            GLX_DRAWABLE_TYPE,      GLX_PIXMAP_BIT,
            GLX_DOUBLEBUFFER,       GL_TRUE,
            GLX_RENDER_TYPE,        GLX_RGBA_BIT,
            GLX_X_RENDERABLE,       GL_TRUE,
            GLX_Y_INVERTED_EXT,     GL_TRUE,
            GLX_RED_SIZE,           8,
            GLX_GREEN_SIZE,         8,
            GLX_BLUE_SIZE,          8,
            GLX_DEPTH_SIZE,         1,
            };

    if (pix_type::rgba == pix_t)
    {
        attributes.push_back(GLX_ALPHA_SIZE);
        attributes.push_back(8);
        attributes.push_back(GLX_BIND_TO_TEXTURE_RGBA_EXT);
        attributes.push_back(GL_TRUE);
    }
    else
    {
        attributes.push_back(GLX_BIND_TO_TEXTURE_RGB_EXT);
        attributes.push_back(GL_TRUE);
    }

    attributes.push_back(GL_NONE);

    int n_fbconfig_attrs {};
    auto fbconfig = glXChooseFBConfig(display_, DefaultScreen(display_), attributes.data(), &n_fbconfig_attrs);
    if (!fbconfig)
    {
        XFreePixmap(display_, p.xpixmap);
        p.xpixmap = 0;
        throw gfx::exception("Cannot get FBConfig.");
    }

    std::vector<int> pixmap_attrs {
        GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
        GLX_MIPMAP_TEXTURE_EXT, GL_FALSE
    };

    pixmap_attrs.push_back(GLX_TEXTURE_FORMAT_EXT);
    pixmap_attrs.push_back(pix_type::rgba == pix_t ? GLX_TEXTURE_FORMAT_RGBA_EXT : GLX_TEXTURE_FORMAT_RGB_EXT);
    pixmap_attrs.push_back(GL_NONE);

    p.glx_pixmap = glXCreatePixmap(display_, fbconfig[0], p.xpixmap, pixmap_attrs.data());

    XFree(fbconfig);

    if (p.glx_pixmap == 0)
    {
        XFreePixmap(display_, p.xpixmap);
        p.xpixmap = 0;
        throw gfx::exception("Cannot create glXCreatePixmap.");
    }

    pixmaps.emplace(++unique_id, p);

    return unique_id;
}

bool context_glx::destroy_pixmap(const pixmap& p) const
{
    if (p == pixmap_invalid_id)
    {
        return false;
    }

    auto it = pixmaps.find(p);
    if (it == std::end(pixmaps))
    {
        assert(false && "Pixmap does not exist");
    }

    auto& glx_pixmap = it->second;

    if (glx_pixmap.glx_pixmap > 0)
    {
        // Destroy glxpixmap
        glXDestroyPixmap(display_, glx_pixmap.glx_pixmap);
        glx_pixmap.glx_pixmap = 0;
    }

    if (glx_pixmap.xpixmap > 0)
    {
        // Destroy xpixmap
        XFreePixmap(display_, glx_pixmap.xpixmap);
        glx_pixmap.xpixmap = 0;
    }

    pixmaps.erase(it);

    return true;
}

bool context_glx::bind_pixmap(const pixmap& p) const
{
    if (p == pixmap_invalid_id)
    {
        return false;
    }

    auto it = pixmaps.find(p);
    if (it == std::end(pixmaps))
    {
        assert(false && "Pixmap does not exist");
    }

    auto& glx_pixmap = it->second;

    if(glx_pixmap.glx_pixmap)
    {
        glXBindTexImageEXT(display_, glx_pixmap.glx_pixmap, GLX_FRONT_EXT, nullptr);
        return true;
    }

    return false;
}

void context_glx::unbind_pixmap(const pixmap& p) const
{
    if (p == pixmap_invalid_id)
    {
        return;
    }

    auto it = pixmaps.find(p);
    if (it == std::end(pixmaps))
    {
        assert(false && "Pixmap does not exist");
    }

    auto& glx_pixmap = it->second;

    glXReleaseTexImageEXT(display_, glx_pixmap.glx_pixmap, GLX_FRONT_EXT);

}

void* context_glx::get_native_handle(const pixmap& p) const
{
    if (p == pixmap_invalid_id)
    {
        return nullptr;
    }

    auto it = pixmaps.find(p);
    if (it == std::end(pixmaps))
    {
        assert(false && "Pixmap does not exist");
    }

    auto& glx_pixmap = it->second;

    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(glx_pixmap.xpixmap));
}

}
