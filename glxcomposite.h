#ifndef GLXCOMPOSITE_H
#define GLXCOMPOSITE_H

#include <stdbool.h>
#include <stddef.h>

typedef unsigned long GLXCID;
typedef GLXCID GLXCWindow;
typedef GLXCID GLXCAtom;
typedef GLXCID GLXCPixmap;

typedef struct GLXCWindowInfo {
    GLXCWindow window;
    GLXCWindow parent;
    bool pixmaps_valid;
    GLXCPixmap x_pixmap;
    GLXCPixmap gl_pixmap;
} GLXCWindowInfo;
typedef struct GLXCWindowAttributes {
    int x;
    int y;
    int width;
    int height;
    bool visible;
} GLXCWindowAttributes;
typedef struct GLXCCompositor GLXCCompositor;

#ifdef __cplusplus
extern "C" {
#endif

    GLXCCompositor* glxc_create_compositor(void);
    int glxc_init_compositor(GLXCCompositor* compositor, const char* display);
    void glxc_destroy_compositor(GLXCCompositor* compositor);
    void glxc_free_compositor(GLXCCompositor* compositor);

    void glxc_init_threads(void);
    void glxc_lock_display(GLXCCompositor* compositor);
    void glxc_unlock_display(GLXCCompositor* compositor);

    GLXCWindow glxc_get_root_window(GLXCCompositor* compositor);
    GLXCWindow glxc_get_composite_window(GLXCCompositor* compositor);

    void glxc_get_window_attrs(GLXCCompositor* compositor, GLXCWindow window, GLXCWindowAttributes* ret);
    GLXCAtom glxc_get_atom(GLXCCompositor* compositor, const char* name);
    GLXCAtom glxc_get_window_type(GLXCCompositor* compositor, GLXCWindow window);
    unsigned long glxc_get_window_desktop(GLXCCompositor* compositor, GLXCWindow window); // Returns the window's workspace

    void glxc_swap_buffers(GLXCCompositor* compositor);

    void (*glxc_get_proc_address(const unsigned char* name))();

    size_t glxc_handle_events(GLXCCompositor* compositor); // Returns the number of events handled
    size_t glxc_get_windows(GLXCCompositor* compositor, const GLXCWindowInfo** ret); // Returns the number of windows

    void glxc_bind_window_texture(GLXCCompositor* compositor, GLXCWindowInfo* window_info);
    void glxc_unbind_window_texture(GLXCCompositor* compositor, const GLXCWindowInfo* window_info);

#ifdef __cplusplus
}
#endif
#endif
