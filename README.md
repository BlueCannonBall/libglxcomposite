# libglxcomposite
Library for writing OpenGL X11 compositors.

## Usage
Start by creating a Compositor object. The Compositor object is the heart of the library, and almost every subroutine in the library operates on it. The Compositor object can be used to get an array of all the child Windows of a given Window (or the root), recursively. To bind the window textures for use with OpenGL, you must first create a GLXPixmap object using the Window object. This object can then be passed to `glx_bind_window_texture(Compositor*, GLXPixmap)` and `glx_unbind_window_texture(Compositor*, GLXPixmap);`. You should avoid recreating these objects between frames, by storing each GLXPixmap object along with its corresponding Window object. You may compare Window arrays between frames to determine when a Window has been destroyed. To know how to draw the Windows, use the various `get_window_*` subroutines.
