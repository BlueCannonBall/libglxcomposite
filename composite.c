#include "composite.h"
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Compositor {
    Display* xdpy;
    Window root;
    Window overlay;
    int screen;
} Compositor;

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
    free(compositor);
}

Window get_root_window(Compositor* compositor) {
    return compositor->root;
}

Window get_composite_window(Compositor* compositor) {
    return compositor->overlay;
}