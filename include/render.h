#ifndef _WXRC_RENDER_H
#define _WXRC_RENDER_H

#include <cglm/cglm.h>
#include <stdbool.h>
#include <GLES2/gl2.h>
#include <openxr/openxr.h>
#include "gltf.h"

struct wxrc_xr_view;
struct wxrc_server;

struct wxrc_gl {
	GLuint grid_program;
	GLuint texture_rgb_program;
	GLuint texture_external_program;
	GLuint gltf_program;

	struct wxrc_gltf_model model;
};

#define WXRC_SURFACE_SCALE 300.0

bool wxrc_gl_init(struct wxrc_gl *gl);
void wxrc_gl_finish(struct wxrc_gl *gl);
void wxrc_gl_render_view(struct wxrc_server *server, struct wxrc_xr_view *view,
	mat4 view_matrix, mat4 projection_matrix);
void wxrc_gl_render_xr_view(struct wxrc_server *server, struct wxrc_xr_view *view,
	XrView *xr_view, GLuint framebuffer, GLuint image, GLuint depth_buffer);

void wxrc_get_projection_matrix(XrView *xr_view, mat4 projection_matrix);

#endif
