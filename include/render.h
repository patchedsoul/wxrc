#ifndef _WXRC_RENDER_H
#define _WXRC_RENDER_H

#include <stdbool.h>
#include <GLES2/gl2.h>
#include <openxr/openxr.h>

struct wxrc_xr_view;
struct wxrc_server;

struct wxrc_gl {
	GLuint grid_program;
};

bool wxrc_gl_init(struct wxrc_gl *gl);
void wxrc_gl_finish(struct wxrc_gl *gl);
void wxrc_gl_render_view(struct wxrc_server *server, struct wxrc_xr_view *view,
	XrView *xr_view, GLuint framebuffer, GLuint image);

#endif
