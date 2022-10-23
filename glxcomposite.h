#ifndef _GLXCOMPOSITE_H
#define _GLXCOMPOSITE_H

#include <stdbool.h>

typedef struct Compositor Compositor;
typedef unsigned long Window;
typedef unsigned long GLXPixmap;

#ifdef __cplusplus
extern "C" {
#endif

    Compositor* create_compositor(const char* display);
    int init_compositor(Compositor* compositor);
    void destroy_compositor(Compositor* compositor);
    void free_compositor(Compositor* compositor);

    void init_threads(void);
    void lock_display(Compositor* compositor);
    void unlock_display(Compositor* compositor);

    Window get_root_window(Compositor* compositor);
    Window get_composite_window(Compositor* compositor);

    int get_window_width(Compositor* compositor, Window window);
    int get_window_height(Compositor* compositor, Window window);
    int get_window_depth(Compositor* compositor, Window window);

    int get_window_x(Compositor* compositor, Window window);
    int get_window_y(Compositor* compositor, Window window);

    bool is_window_visible(Compositor* compositor, Window window);

    void swap_buffers(Compositor* compositor);

    int get_windows_recursive(Compositor* compositor, Window parent, Window** windows, unsigned int* nwindows);
    int get_all_windows(Compositor* compositor, Window** windows_ret, unsigned int* nwindows_ret);
    void free_windows(Window* windows);

    GLXPixmap create_glx_pixmap(Compositor* compositor, Window window);
    void destroy_glx_pixmap(Compositor* compositor, GLXPixmap glx_pixmap);
    void glx_bind_window_texture(Compositor* compositor, GLXPixmap glx_pixmap);
    void glx_unbind_window_texture(Compositor* compositor, GLXPixmap glx_pixmap);

#ifdef __cplusplus
}
#endif
#endif
