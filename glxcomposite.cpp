#include "glxcomposite.h"
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

#define MAX_PROPERTY_VALUE_SIZE 4096

typedef void (*glXBindTexImageEXTProc)(Display*, GLXDrawable, int, const int*);
typedef void (*glXReleaseTexImageEXTProc)(Display*, GLXDrawable, int);

struct GLXCCompositor {
    Display* display;
    int screen;
    GLXCWindow root;
    GLXCWindow overlay;

    GLXContext ctx;
    glXBindTexImageEXTProc glXBindTexImageEXT;
    glXReleaseTexImageEXTProc glXReleaseTexImageEXT;
    GLXFBConfig* fbcs;
    int fbc_count;

    std::vector<GLXCWindowInfo> windows;
};

bool operator==(const GLXCWindowInfo& a, const GLXCWindowInfo& b) {
    return a.window == b.window;
}

bool operator!=(const GLXCWindowInfo& a, const GLXCWindowInfo& b) {
    return a.window != b.window;
}

bool operator==(const GLXCWindowInfo& window_info, GLXCWindow window) {
    return window_info.window == window;
}

bool operator!=(const GLXCWindowInfo& window_info, GLXCWindow window) {
    return window_info.window == window;
}

int x_error_handler(Display* display, XErrorEvent* event) {
    char error_string[128];
    XGetErrorText(display, event->error_code, error_string, sizeof error_string);
    std::cerr << "libglxcomposite: An X error has occured: " << error_string << " (error: " << event->error_code << ", major: " << event->request_code << ", minor: " << event->minor_code << ')' << std::endl;
    return 0; // This value is ignored
}

GLXCCompositor* glxc_create_compositor() {
    GLXCCompositor* compositor = new GLXCCompositor;
    return compositor;
}

int glxc_init_compositor(GLXCCompositor* compositor, const char* display) {
    compositor->display = NULL;
    if (!(compositor->display = XOpenDisplay(display))) {
        std::cerr << "libglxcomposite: Failed to open display" << std::endl;
        return 1;
    }
    compositor->screen = DefaultScreen(compositor->display);
    compositor->root = ScreenOfDisplay(compositor->display, compositor->screen)->root;
    compositor->overlay = XCompositeGetOverlayWindow(compositor->display, compositor->root);
    XSetErrorHandler(&x_error_handler);

    XCompositeRedirectSubwindows(compositor->display, compositor->root, CompositeRedirectAutomatic);

    XserverRegion region = XFixesCreateRegion(compositor->display, NULL, 0);
    XFixesSetWindowShapeRegion(compositor->display, compositor->overlay, ShapeBounding, 0, 0, 0);
    XFixesSetWindowShapeRegion(compositor->display, compositor->overlay, ShapeInput, 0, 0, region);
    XFixesDestroyRegion(compositor->display, region);

    typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

    const static int visual_attrs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_DOUBLEBUFFER, true, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1, None};

    int fbc_count;
    GLXFBConfig* fbc = glXChooseFBConfig(compositor->display,
        compositor->screen,
        visual_attrs,
        &fbc_count);
    if (!fbc) {
        std::cerr << "libglxcomposite: glXChooseFBConfig() failed" << std::endl;
        return 1;
    }

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc) glXGetProcAddress((const unsigned char*) "glXCreateContextAttribsARB");
    compositor->glXBindTexImageEXT = (glXBindTexImageEXTProc) glXGetProcAddress((const unsigned char*) "glXBindTexImageEXT");
    compositor->glXReleaseTexImageEXT = (glXReleaseTexImageEXTProc) glXGetProcAddress((const unsigned char*) "glXReleaseTexImageEXT");

    if (!glXCreateContextAttribsARB) {
        std::cerr << "libglxcomposite: glXCreateContextAttribsARB() not found" << std::endl;
        XFree(fbc);
        return 1;
    }

    const static int context_attrs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB,
        3,
        GLX_CONTEXT_MINOR_VERSION_ARB,
        3,
        None,
    };
    compositor->ctx = glXCreateContextAttribsARB(compositor->display, fbc[0], NULL, true, context_attrs);
    glXMakeCurrent(compositor->display, compositor->overlay, compositor->ctx);
    compositor->fbcs = glXGetFBConfigs(compositor->display, compositor->screen, &compositor->fbc_count);
    XFree(fbc);

    {
        Window root;
        Window parent;
        Window* children;
        unsigned int child_count;
        XQueryTree(compositor->display, compositor->root, &root, &parent, &children, &child_count);

        compositor->windows.reserve(child_count);
        for (unsigned int i = 0; i < child_count; ++i) {
            GLXCWindowInfo window_info;
            window_info.window = children[i];
            window_info.parent = root;
            window_info.pixmaps_valid = false;
            compositor->windows.push_back(window_info);
        }
    }
    XSelectInput(compositor->display, compositor->root, SubstructureNotifyMask);

    return 0;
}

void glxc_destroy_compositor(GLXCCompositor* compositor) {
    for (auto& window_info : compositor->windows) {
        if (window_info.pixmaps_valid) {
            XFreePixmap(compositor->display, window_info.x_pixmap);
            glXDestroyPixmap(compositor->display, window_info.gl_pixmap);
        }
    }

    glXDestroyContext(compositor->display, compositor->ctx);
    XCompositeUnredirectSubwindows(compositor->display, compositor->root, CompositeRedirectAutomatic);
    XCloseDisplay(compositor->display);
}

void glxc_free_compositor(GLXCCompositor* compositor) {
    delete compositor;
}

void glxc_init_threads() {
    XInitThreads();
}

void glxc_lock_display(GLXCCompositor* compositor) {
    XLockDisplay(compositor->display);
}

void glxc_unlock_display(GLXCCompositor* compositor) {
    XUnlockDisplay(compositor->display);
}

GLXCWindow glxc_get_root_window(GLXCCompositor* compositor) {
    return compositor->root;
}

GLXCWindow glxc_get_composite_window(GLXCCompositor* compositor) {
    return compositor->overlay;
}

void glxc_get_window_attrs(GLXCCompositor* compositor, GLXCWindow window, GLXCWindowAttributes* ret) {
    GLXCWindow child;
    XTranslateCoordinates(compositor->display, window, compositor->root, 0, 0, &ret->x, &ret->y, &child);

    XWindowAttributes attrs;
    XGetWindowAttributes(compositor->display, window, &attrs);
    ret->width = attrs.width;
    ret->width = attrs.height;
    ret->visible = attrs.map_state == IsViewable;
}

GLXCAtom glxc_get_atom(GLXCCompositor* compositor, const char* name) {
    return XInternAtom(compositor->display, name, False);
}

GLXCAtom glxc_get_window_type(GLXCCompositor* compositor, GLXCWindow window) {
    GLXCAtom type;
    int format;
    unsigned long size;
    unsigned long bytes_after;
    unsigned char* ret_bytes;
    XGetWindowProperty(compositor->display, window, glxc_get_atom(compositor, "_NET_WM_WINDOW_TYPE"), 0, MAX_PROPERTY_VALUE_SIZE / 4, False, XA_ATOM, &type, &format, &size, &bytes_after, &ret_bytes);

    GLXCAtom ret = *((GLXCAtom*) ret_bytes);
    XFree(ret_bytes);
    return ret;
}

unsigned long glxc_get_window_desktop(GLXCCompositor* compositor, GLXCWindow window) {
    GLXCAtom type;
    int format;
    unsigned long size;
    unsigned long bytes_after;
    unsigned char* ret_bytes;
    XGetWindowProperty(compositor->display, window, glxc_get_atom(compositor, "_NET_WM_DESKTOP"), 0, MAX_PROPERTY_VALUE_SIZE / 4, False, XA_CARDINAL, &type, &format, &size, &bytes_after, &ret_bytes);

    unsigned long ret = *((unsigned long*) ret_bytes);
    XFree(ret_bytes);
    return ret;
}

void glxc_swap_buffers(GLXCCompositor* compositor) {
    glXSwapBuffers(compositor->display, compositor->overlay);
}

void (*glxc_get_proc_address(const unsigned char* name))() {
    return glXGetProcAddress(name);
}

size_t glxc_handle_events(GLXCCompositor* compositor) {
    size_t ret;
    for (ret = 0; QLength(compositor->display); ++ret) {
        XEvent event;
        XNextEvent(compositor->display, &event);

        switch (event.type) {
        case CreateNotify: {
            GLXCWindowInfo window_info;
            window_info.window = event.xcreatewindow.window;
            window_info.parent = event.xcreatewindow.parent;
            window_info.pixmaps_valid = false;
            compositor->windows.push_back(window_info);
            break;
        }

        case ReparentNotify: {
            auto window_info_it = std::find(compositor->windows.begin(), compositor->windows.end(), event.xreparent.window);
            window_info_it->parent = event.xreparent.parent;
            break;
        }

        case ConfigureNotify: {
            auto window_info_it = std::find(compositor->windows.begin(), compositor->windows.end(), event.xconfigure.window);
            GLXCWindowInfo window_info = *window_info_it;
            compositor->windows.erase(window_info_it);

            auto above_window_info_it = std::find(compositor->windows.begin(), compositor->windows.end(), event.xconfigure.above);
            compositor->windows.insert(std::next(above_window_info_it), window_info);
            break;
        }

        case CirculateNotify: {
            std::stable_sort(compositor->windows.begin(), compositor->windows.end(), [&event](const auto& a, const auto& b) {
                if (a == event.xcirculate.window) {
                    return event.xcirculate.place == PlaceOnBottom;
                } else if (b == event.xcirculate.window) {
                    return event.xcirculate.place == PlaceOnTop;
                } else {
                    return false;
                }
            });
            break;
        }

        case MapNotify:
        case UnmapNotify: {
            GLXCWindow window;
            if (event.type == MapNotify) {
                window = event.xmap.window;
            } else if (event.type == UnmapNotify) {
                window = event.xunmap.window;
            } else {
                throw std::logic_error("Invalid event type");
            }
            auto window_info_it = std::find(compositor->windows.begin(), compositor->windows.end(), window);
            if (window_info_it->pixmaps_valid) {
                XFreePixmap(compositor->display, window_info_it->x_pixmap);
                glXDestroyPixmap(compositor->display, window_info_it->gl_pixmap);
                window_info_it->pixmaps_valid = false;
            }
            break;
        }

        case DestroyNotify: {
            auto window_info_it = std::find(compositor->windows.begin(), compositor->windows.end(), event.xdestroywindow.window);
            if (window_info_it->pixmaps_valid) {
                XFreePixmap(compositor->display, window_info_it->x_pixmap);
                glXDestroyPixmap(compositor->display, window_info_it->gl_pixmap);
            }
            compositor->windows.erase(window_info_it);
            break;
        }
        }
    }
    return ret;
}

size_t glxc_get_windows(GLXCCompositor* compositor, GLXCWindowInfo** ret) {
    *ret = compositor->windows.data();
    return compositor->windows.size();
}

void glxc_bind_window_texture(GLXCCompositor* compositor, GLXCWindowInfo* window_info) {
    if (!window_info->pixmaps_valid) {
        XWindowAttributes attrs;
        XGetWindowAttributes(compositor->display, window_info->window, &attrs);

        int format;
        GLXFBConfig fbc;

        bool found = false;
        for (int i = 0; i < compositor->fbc_count; ++i) {
            fbc = compositor->fbcs[i];

            int has_alpha;
            glXGetFBConfigAttrib(compositor->display, fbc, GLX_BIND_TO_TEXTURE_RGBA_EXT, &has_alpha);

            XVisualInfo* visual = glXGetVisualFromFBConfig(compositor->display, fbc);
            if (attrs.depth != visual->depth) {
                XFree(visual);
                continue;
            }
            XFree(visual);

            format = has_alpha ? GLX_TEXTURE_FORMAT_RGBA_EXT : GLX_TEXTURE_FORMAT_RGB_EXT;
            found = true;
            break;
        }
        if (!found) {
            throw std::runtime_error("No suitable format found");
        }

        const int pixmap_attributes[] = {
            GLX_TEXTURE_TARGET_EXT,
            GLX_TEXTURE_2D_EXT,
            GLX_TEXTURE_FORMAT_EXT,
            format,
            None,
        };
        window_info->x_pixmap = XCompositeNameWindowPixmap(compositor->display, window_info->window);
        window_info->gl_pixmap = glXCreatePixmap(compositor->display, fbc, window_info->x_pixmap, pixmap_attributes);
        window_info->pixmaps_valid = true;
    }

    compositor->glXBindTexImageEXT(compositor->display, window_info->gl_pixmap, GLX_FRONT_LEFT_EXT, NULL);
}

void glxc_unbind_window_texture(GLXCCompositor* compositor, const GLXCWindowInfo* window_info) {
    compositor->glXReleaseTexImageEXT(compositor->display, window_info->gl_pixmap, GLX_FRONT_LEFT_EXT);
}
