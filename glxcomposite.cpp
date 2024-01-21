#include "glxcomposite.h"
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/X.h>
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

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef void (*glXBindTexImageEXTProc)(Display*, GLXDrawable, int, const int*);
typedef void (*glXReleaseTexImageEXTProc)(Display*, GLXDrawable, int);

struct GLXCCompositor {
    Display* display;
    int screen;
    GLXCWindow root;
    GLXCWindow overlay;
    Visual* visual;

    GLXContext ctx;
    glXBindTexImageEXTProc glXBindTexImageEXT;
    glXReleaseTexImageEXTProc glXReleaseTexImageEXT;
    GLXFBConfig* fb_configs;
    int fb_config_count;

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
    std::cerr << "libglxcomposite: An X error has occured: " << error_string << " (error: " << +event->error_code << ", major: " << +event->request_code << ", minor: " << +event->minor_code << ')' << std::endl;
    return 0; // This value is ignored
}

GLXCCompositor* glxc_create_compositor() {
    return new GLXCCompositor;
}

int glxc_init_compositor(GLXCCompositor* compositor, const char* display) {
    compositor->display = nullptr;
    if (!(compositor->display = XOpenDisplay(display))) {
        std::cerr << "libglxcomposite: Failed to open display" << std::endl;
        return 1;
    }
    XSetErrorHandler(&x_error_handler);
    compositor->screen = DefaultScreen(compositor->display);
    compositor->root = RootWindow(compositor->display, compositor->screen);
    compositor->overlay = XCompositeGetOverlayWindow(compositor->display, compositor->root);
    compositor->visual = DefaultVisual(compositor->display, compositor->screen);

    XCompositeRedirectSubwindows(compositor->display, compositor->root, CompositeRedirectManual);

    XserverRegion region = XFixesCreateRegion(compositor->display, nullptr, 0);
    XFixesSetWindowShapeRegion(compositor->display, compositor->overlay, ShapeBounding, 0, 0, 0);
    XFixesSetWindowShapeRegion(compositor->display, compositor->overlay, ShapeInput, 0, 0, region);
    XFixesDestroyRegion(compositor->display, region);

    GLXFBConfig fb_config;
    compositor->fb_configs = glXGetFBConfigs(compositor->display, compositor->screen, &compositor->fb_config_count);
    {
        int target_visual_id = XVisualIDFromVisual(compositor->visual);
        for (int i = 0; i < compositor->fb_config_count; ++i) {
            int visual_id;
            if (glXGetFBConfigAttrib(compositor->display, compositor->fb_configs[i], GLX_VISUAL_ID, &visual_id) == Success && visual_id == target_visual_id) {
                fb_config = compositor->fb_configs[i];
                break;
            }
        }
    }

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc) glXGetProcAddress((const unsigned char*) "glXCreateContextAttribsARB");
    if (!glXCreateContextAttribsARB) {
        std::cerr << "libglxcomposite: glXCreateContextAttribsARB() not found" << std::endl;
        return 1;
    }
    static constexpr int context_attrs[] = {
        GLX_CONTEXT_FLAGS_ARB,
        GLX_CONTEXT_DEBUG_BIT_ARB,
        None,
    };
    compositor->ctx = glXCreateContextAttribsARB(compositor->display, fb_config, nullptr, True, context_attrs);
    glXMakeCurrent(compositor->display, compositor->overlay, compositor->ctx);

    compositor->glXBindTexImageEXT = (glXBindTexImageEXTProc) glXGetProcAddress((const unsigned char*) "glXBindTexImageEXT");
    compositor->glXReleaseTexImageEXT = (glXReleaseTexImageEXTProc) glXGetProcAddress((const unsigned char*) "glXReleaseTexImageEXT");

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
    XCompositeUnredirectSubwindows(compositor->display, compositor->root, CompositeRedirectManual);
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
                if (a.parent == b.parent) {
                    if (a == event.xcirculate.window) {
                        return event.xcirculate.place == PlaceOnBottom;
                    } else if (b == event.xcirculate.window) {
                        return event.xcirculate.place == PlaceOnTop;
                    }
                }
                return false;
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
        std::cout << compositor->fb_config_count << std::endl;

        GLXFBConfig* fb_config;
        bool found = false;
        for (fb_config = compositor->fb_configs; fb_config != compositor->fb_configs + compositor->fb_config_count; ++fb_config) {
            int has_alpha;
            if (glXGetFBConfigAttrib(compositor->display, *fb_config, GLX_BIND_TO_TEXTURE_RGBA_EXT, &has_alpha) != Success || !has_alpha) {
                continue;
            }

            int samples;
            if (glXGetFBConfigAttrib(compositor->display, *fb_config, GLX_SAMPLES, &samples) != Success || samples > 1) {
                continue;
            }

            int texture_targets;
            if (glXGetFBConfigAttrib(compositor->display, *fb_config, GLX_BIND_TO_TEXTURE_TARGETS_EXT, &texture_targets) != Success || !(texture_targets & GLX_TEXTURE_2D_EXT)) {
                continue;
            }

            found = true;
            break;
        }
        if (!found) {
            throw std::runtime_error("No suitable format found");
        }

        static constexpr int pixmap_attrs[] = {
            GLX_TEXTURE_TARGET_EXT,
            GLX_TEXTURE_2D_EXT,
            GLX_TEXTURE_FORMAT_EXT,
            GLX_TEXTURE_FORMAT_RGBA_EXT,
            None,
        };
        window_info->x_pixmap = XCompositeNameWindowPixmap(compositor->display, window_info->window);
        window_info->gl_pixmap = glXCreatePixmap(compositor->display, *fb_config, window_info->x_pixmap, pixmap_attrs);
        window_info->pixmaps_valid = true;
    }

    compositor->glXBindTexImageEXT(compositor->display, window_info->gl_pixmap, GLX_FRONT_LEFT_EXT, nullptr);
}

void glxc_unbind_window_texture(GLXCCompositor* compositor, const GLXCWindowInfo* window_info) {
    compositor->glXReleaseTexImageEXT(compositor->display, window_info->gl_pixmap, GLX_FRONT_LEFT_EXT);
}
