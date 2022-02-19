#ifndef _COMPOSITE_H
#define _COMPOSITE_H

#include <X11/X.h>

typedef struct Compositor Compositor;

#ifdef __cplusplus
extern "C" {
#endif

    Compositor* compositor_create(const char* display);
    int compositor_init(Compositor* compositor);
    void compositor_destroy(Compositor* compositor);
    void compositor_free(Compositor* compositor);

    Window get_root_window(Compositor* compositor);
    Window get_composite_window(Compositor* compositor);

    int get_all_windows(Compositor* compositor, Window** windows_ret, unsigned int* nwindows_ret);
    void free_windows(Window* windows);

#ifdef __cplusplus
}
#endif
#endif