#include "glxcomposite.h"
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROPERTY_VALUE_LEN 4096

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

    int x_error_handler(Display* xdpy, XErrorEvent* ev) {
        char error_string[128];
        XGetErrorText(xdpy, ev->error_code, error_string, sizeof(error_string));
        fprintf(stderr, "libglxcomposite: An X error has occured: %s (error=%u, major=%u, minor=%u)\n", error_string, ev->error_code, ev->request_code, ev->minor_code);
        return 0; /* This value is ignored */
    }

    int init_compositor(Compositor* compositor) {
        XSetErrorHandler(&x_error_handler);

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

    void init_threads(void) {
        XInitThreads();
    }

    void lock_display(Compositor* compositor) {
        XLockDisplay(compositor->xdpy);
    }

    void unlock_display(Compositor* compositor) {
        XUnlockDisplay(compositor->xdpy);
    }

    Window get_root_window(Compositor* compositor) {
        return compositor->root;
    }

    Window get_composite_window(Compositor* compositor) {
        return compositor->overlay;
    }

    int get_window_width(Compositor* compositor, Window window) {
        XWindowAttributes attribs;
        XGetWindowAttributes(compositor->xdpy, window, &attribs);
        return attribs.width;
    }

    int get_window_height(Compositor* compositor, Window window) {
        XWindowAttributes attribs;
        XGetWindowAttributes(compositor->xdpy, window, &attribs);
        return attribs.height;
    }

    int get_window_depth(Compositor* compositor, Window window) {
        XWindowAttributes attribs;
        XGetWindowAttributes(compositor->xdpy, window, &attribs);
        return attribs.depth;
    }

    int get_window_x(Compositor* compositor, Window window) {
        XWindowAttributes attribs;
        XGetWindowAttributes(compositor->xdpy, window, &attribs);
        return attribs.x;
    }

    int get_window_y(Compositor* compositor, Window window) {
        XWindowAttributes attribs;
        XGetWindowAttributes(compositor->xdpy, window, &attribs);
        return attribs.y;
    }

    Atom get_window_type(Compositor* compositor, Window window) {
        Atom type;
        int format;
        unsigned long size;
        unsigned long bytes_after;
        unsigned char* ret_bytes;

        XGetWindowProperty(compositor->xdpy, window, XInternAtom(compositor->xdpy, "_NET_WM_WINDOW_TYPE", False), 0, MAX_PROPERTY_VALUE_LEN / 4, False, XA_ATOM, &type, &format, &size, &bytes_after, &ret_bytes);

        Atom ret = *((Atom*) ret_bytes);
        XFree(ret_bytes);
        return ret;
    }

    unsigned long get_window_desktop(Compositor* compositor, Window window) {
        Atom type;
        int format;
        unsigned long size;
        unsigned long bytes_after;
        unsigned char* ret_bytes;

        XGetWindowProperty(compositor->xdpy, window, XInternAtom(compositor->xdpy, "_NET_WM_DESKTOP", False), 0, MAX_PROPERTY_VALUE_LEN / 4, False, XA_CARDINAL, &type, &format, &size, &bytes_after, &ret_bytes);

        unsigned long ret = *((unsigned long*) ret_bytes);
        XFree(ret_bytes);
        return ret;
    }

    bool is_window_visible(Compositor* compositor, Window window) {
        XWindowAttributes attribs;
        XGetWindowAttributes(compositor->xdpy, window, &attribs);
        return attribs.map_state == IsViewable;
    }

    void swap_buffers(Compositor* compositor) {
        glXSwapBuffers(compositor->xdpy, compositor->overlay);
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
        if (children == NULL || nchildren == 0) {
            return 0;
        }

        *windows = realloc(*windows, (*nwindows + nchildren) * sizeof(Window));
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
        free(windows);
    }

    void (*glx_get_proc_address(const GLubyte* name))() {
        return glXGetProcAddress(name);
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

    void destroy_glx_pixmap(Compositor* compositor, GLXPixmap glx_pixmap) {
        glXDestroyPixmap(compositor->xdpy, glx_pixmap);
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
