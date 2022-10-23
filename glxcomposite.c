#include "glxcomposite.h"
#include <GL/glx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*glXBindTexImageEXTProc)(Display*, GLXDrawable, int, const int*);
typedef void (*glXReleaseTexImageEXTProc)(Display*, GLXDrawable, int);

typedef struct Compositor {
    Display* xdpy;
    Window root;
    Window overlay;
    int screen;
    glXBindTexImageEXTProc glXBindTexImageEXT;
    glXReleaseTexImageEXTProc glXReleaseTexImageEXT;
    GLXFBConfig* fbcs;
    int nfbcs;
} Compositor;

#ifdef __cplusplus
extern "C" {
#endif

    Compositor* create_compositor(const char* display) {
        Compositor* compositor = malloc(sizeof(Compositor));

        if ((compositor->xdpy = XOpenDisplay(display)) == NULL) {
            fprintf(stderr, "libglxcomposite: Failed to open display\n");
            return NULL;
        }
        compositor->root = DefaultRootWindow(compositor->xdpy);
        compositor->overlay = XCompositeGetOverlayWindow(compositor->xdpy, compositor->root);
        compositor->screen = DefaultScreen(compositor->xdpy);

        return compositor;
    }

    int init_compositor(Compositor* compositor) {
        XCompositeRedirectSubwindows(compositor->xdpy, compositor->root, CompositeRedirectAutomatic);

        XserverRegion region = XFixesCreateRegion(compositor->xdpy, NULL, 0);
        XFixesSetWindowShapeRegion(compositor->xdpy, compositor->overlay, ShapeBounding, 0, 0, 0);
        XFixesSetWindowShapeRegion(compositor->xdpy, compositor->overlay, ShapeInput, 0, 0, region);
        XFixesDestroyRegion(compositor->xdpy, region);

        typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

        const static int visual_attribs[] = {
            GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_DOUBLEBUFFER, true, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1, None};

        int num_fbc = 0;
        GLXFBConfig* fbc = glXChooseFBConfig(compositor->xdpy,
            compositor->screen,
            visual_attribs,
            &num_fbc);
        if (!fbc) {
            fprintf(stderr, "libglxcomposite: glXChooseFBConfig() failed\n");
            return 1;
        }

        glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc) glXGetProcAddress((const GLubyte*) "glXCreateContextAttribsARB");
        compositor->glXBindTexImageEXT = (glXBindTexImageEXTProc) glXGetProcAddress((const GLubyte*) "glXBindTexImageEXT");
        compositor->glXReleaseTexImageEXT = (glXReleaseTexImageEXTProc) glXGetProcAddress((const GLubyte*) "glXReleaseTexImageEXT");

        if (!glXCreateContextAttribsARB) {
            fprintf(stderr, "libglxcomposite: glXCreateContextAttribsARB() not found\n");
        }

        const static int context_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3, GLX_CONTEXT_MINOR_VERSION_ARB, 3, None};
        GLXContext ctx = glXCreateContextAttribsARB(compositor->xdpy, fbc[0], NULL, true, context_attribs);

        glXMakeCurrent(compositor->xdpy, compositor->overlay, ctx);

        compositor->fbcs = glXGetFBConfigs(compositor->xdpy, compositor->screen, &compositor->nfbcs);

        return 0;
    }

    void destroy_compositor(Compositor* compositor) {
        XCloseDisplay(compositor->xdpy);
    }

    void free_compositor(Compositor* compositor) {
        free(compositor);
    }

    Window get_root_window(Compositor* compositor) {
        return compositor->root;
    }

    Window get_composite_window(Compositor* compositor) {
        return compositor->overlay;
    }

    int get_windows_recursive(Compositor* compositor, Window parent, Window** windows, unsigned int* nwindows) {
        Window root_ret;
        Window parent_ret;

        Window* children;
        unsigned int nchildren;

        Status status = XQueryTree(
            compositor->xdpy,
            parent,
            &root_ret,
            &parent_ret,
            &children,
            &nchildren);

        if (status == 0) {
            fprintf(stderr, "libglxcomposite: XQueryTree() failed\n");
            return 1;
        }
        if (nchildren == 0 || children == NULL) {
            return 0;
        }

        *windows = realloc(*windows, *nwindows + nchildren);
        memcpy(*windows + *nwindows, children, nchildren * sizeof(Window));
        *nwindows += nchildren;

        unsigned int i;
        for (i = 0; i < nchildren; i++) {
            if (get_windows_recursive(compositor, children[i], windows, nwindows)) {
                return 1;
            }
        }

        return 0;
    }

    int get_all_windows(Compositor* compositor, Window** windows_ret, unsigned int* nwindows_ret) {
        *windows_ret = malloc(0);
        *nwindows_ret = 0;
        return get_windows_recursive(compositor, compositor->root, windows_ret, nwindows_ret);
    }

    void free_windows(Window* windows) {
        XFree(windows);
    }

    GLXPixmap create_glx_pixmap(Compositor* compositor, Window window) {
        XWindowAttributes attribs;
        XGetWindowAttributes(compositor->xdpy, window, &attribs);

        int format = format;
        GLXFBConfig fbc;

        int i;
        for (i = 0; i < compositor->nfbcs; i++) {
            fbc = compositor->fbcs[i];

            int has_alpha;
            glXGetFBConfigAttrib(compositor->xdpy, fbc, GLX_BIND_TO_TEXTURE_RGBA_EXT, &has_alpha);

            XVisualInfo* visual = glXGetVisualFromFBConfig(compositor->xdpy, fbc);
            if (attribs.depth != visual->depth) {
                XFree(visual);
                continue;
            }
            XFree(visual);

            format = has_alpha ? GLX_TEXTURE_FORMAT_RGBA_EXT : GLX_TEXTURE_FORMAT_RGB_EXT;
            break;
        }

        const int pixmap_attributes[] = {
            GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT, GLX_TEXTURE_FORMAT_EXT, format, None /* GLX_TEXTURE_FORMAT_RGB_EXT */
        };

        return glXCreatePixmap(compositor->xdpy, fbc, XCompositeNameWindowPixmap(compositor->xdpy, window), pixmap_attributes);
    }

    void glx_bind_window_texture(Compositor* compositor, GLXPixmap glx_pixmap) {
        compositor->glXBindTexImageEXT(compositor->xdpy, glx_pixmap, GLX_FRONT_LEFT_EXT, NULL);
    }

    void glx_unbind_window_texture(Compositor* compositor, GLXPixmap glx_pixmap) {
        compositor->glXReleaseTexImageEXT(compositor->xdpy, glx_pixmap, GLX_FRONT_LEFT_EXT);
    }

#ifdef __cplusplus
}
#endif
