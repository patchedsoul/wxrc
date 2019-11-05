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

/* TODO: remove these private wlroots definitions */

enum wlr_gles2_texture_type {
	WLR_GLES2_TEXTURE_GLTEX,
	WLR_GLES2_TEXTURE_WL_DRM_GL,
	WLR_GLES2_TEXTURE_WL_DRM_EXT,
	WLR_GLES2_TEXTURE_DMABUF,
};

struct wlr_gles2_texture {
	struct wlr_texture wlr_texture;

	struct wlr_egl *egl;
	enum wlr_gles2_texture_type type;
	int width, height;
	bool has_alpha;
	enum wl_shm_format wl_format; // used to interpret upload data
	bool inverted_y;

	// Not set if WLR_GLES2_TEXTURE_GLTEX
	EGLImageKHR image;
	GLuint image_tex;

	union {
		GLuint gl_tex;
		struct wl_resource *wl_drm;
	};
};

bool wxrc_gl_init(struct wxrc_gl *gl);
void wxrc_gl_finish(struct wxrc_gl *gl);
void wxrc_gl_render_view(struct wxrc_server *server, struct wxrc_xr_view *view,
	XrView *xr_view, GLuint framebuffer, GLuint image);

#endif
