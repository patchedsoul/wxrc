#ifndef _WXRC_RENDER_H
#define _WXRC_RENDER_H

#include <stdbool.h>
#include <GLES2/gl2.h>
#include <openxr/openxr.h>
#include <wlr/render/wlr_texture.h>

struct wxrc_xr_view;
struct wxrc_server;

struct wxrc_gl {
	GLuint grid_program;
	GLuint texture_program;
};

/* TODO: remove this private wlroots definition */

struct wlr_gles2_texture {
	struct wlr_texture wlr_texture;
	struct wlr_egl *egl;

	// Basically:
	//   GL_TEXTURE_2D == mutable
	//   GL_TEXTURE_EXTERNAL_OES == immutable
	GLenum target;
	GLuint tex;

	EGLImageKHR image;

	int width, height;
	bool inverted_y;
	bool has_alpha;

	// Only affects target == GL_TEXTURE_2D
	enum wl_shm_format wl_format; // used to interpret upload data
};

bool wxrc_gl_init(struct wxrc_gl *gl);
void wxrc_gl_finish(struct wxrc_gl *gl);
void wxrc_gl_render_view(struct wxrc_server *server, struct wxrc_xr_view *view,
	XrView *xr_view, GLuint framebuffer, GLuint image);

#endif
