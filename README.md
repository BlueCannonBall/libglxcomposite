# libglxcomposite
Library for writing OpenGL X11 compositors.

## Usage
Start by creating a `GLXCCompositor` object. The `GLXCCompositor` object is the heart of the library, and almost every subroutine in the library operates on it. The `size_t glxc_get_windows(GLXCCompositor* compositor, const GLXCWindowInfo** ret)` subroutine can be used to get a list of all existing `GLXCWindowInfo` objects, as well as their properties (position, dimensions, etc). These objects can then be passed to `glxc_bind_window_texture(GLXCCompositor*, GLXCWindowInfo*)` and `glx_unbind_window_texture(GLXCCompositor*, GLXCWindowInfo*)` to bind their textures. Make sure to call `glxc_handle_events(GLXCCompositor*)` between frames to keep the internal window list updated.
