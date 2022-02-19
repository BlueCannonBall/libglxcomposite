#include "composite.h"
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Compositor {
    Display* xdpy;
    Window root;
    Window overlay;
    int screen;
} Compositor;

#ifdef __cplusplus
extern "C" {
#endif

    Compositor* compositor_create(const char* display) {
        Compositor* compositor = malloc(sizeof(Compositor));

        if ((compositor->xdpy = XOpenDisplay(display)) == NULL) {
            fprintf(stderr, "libcomposite: Failed to open display\n");
            return NULL;
        }
        compositor->root = DefaultRootWindow(compositor->xdpy);
        compositor->overlay = XCompositeGetOverlayWindow(compositor->xdpy, compositor->root);
        compositor->screen = DefaultScreen(compositor->xdpy);

        return compositor;
    }

    int compositor_init(Compositor* compositor) {
        XCompositeRedirectSubwindows(compositor->xdpy, compositor->root, CompositeRedirectAutomatic);

        XserverRegion region = XFixesCreateRegion(compositor->xdpy, NULL, 0);
        XFixesSetWindowShapeRegion(compositor->xdpy, compositor->overlay, ShapeBounding, 0, 0, 0);
        XFixesSetWindowShapeRegion(compositor->xdpy, compositor->overlay, ShapeInput, 0, 0, region);
        XFixesDestroyRegion(compositor->xdpy, region);

        typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

        static int visual_attribs[] = {
            GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_DOUBLEBUFFER, true, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1, None};

        int num_fbc = 0;
        GLXFBConfig* fbc = glXChooseFBConfig(compositor->xdpy,
            compositor->screen,
            visual_attribs,
            &num_fbc);
        if (!fbc) {
            fprintf(stderr, "libcomposite: glXChooseFBConfig() failed\n");
            return 1;
        }

        glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
        glXCreateContextAttribsARB =
            (glXCreateContextAttribsARBProc)
                glXGetProcAddress((const GLubyte*) "glXCreateContextAttribsARB");

        if (!glXCreateContextAttribsARB) {
            fprintf(stderr, "libcomposite: glXCreateContextAttribsARB() not found\n");
        }

        static int context_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3, GLX_CONTEXT_MINOR_VERSION_ARB, 3, None};
        GLXContext ctx = glXCreateContextAttribsARB(compositor->xdpy, fbc[0], NULL, true, context_attribs);

        glXMakeCurrent(compositor->xdpy, compositor->overlay, ctx);

        return 0;
    }

    void compositor_destroy(Compositor* compositor) {
        XCloseDisplay(compositor->xdpy);
    }

    void compositor_free(Compositor* compositor) {
        free(compositor);
    }

    Window get_root_window(Compositor* compositor) {
        return compositor->root;
    }

    Window get_composite_window(Compositor* compositor) {
        return compositor->overlay;
    }

    int recursive_get_all_windows(Compositor* compositor, Window parent, Window** windows, unsigned int* nwindows) {
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
            fprintf(stderr, "libcomposite: XQueryTree() failed\n");
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
            if (recursive_get_all_windows(compositor, children[i], windows, nwindows) == 1) {
                return 1;
            }
        }

        return 0;
    }

    int get_all_windows(Compositor* compositor, Window** windows_ret, unsigned int* nwindows_ret) {
        *windows_ret = malloc(0);
        *nwindows_ret = 0;

        return recursive_get_all_windows(compositor, compositor->root, windows_ret, nwindows_ret);
    }

    void free_windows(Window* windows) {
        XFree(windows);
    }

#ifdef __cplusplus
}
#endif