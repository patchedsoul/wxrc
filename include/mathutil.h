#ifndef _WXRC_MATHUTIL_H
#define _WXRC_MATHUTIL_H

#include <cglm/cglm.h>

struct wlr_surface;
struct wxrc_view;

void wxrc_mat4_rotate(mat4 m, vec3 angles);
bool wxrc_intersect_plane_line(vec3 plane_point, vec3 plane_normal,
	vec3 line_point, vec3 line_dir, vec3 intersection);
bool wxrc_intersect_surface_plane_line(struct wlr_surface *surface,
	mat4 model_matrix, vec3 surface_position, vec3 surface_rotation,
	vec3 line_position, vec3 line_dir, vec3 intersection,
	float *sx_ptr, float *sy_ptr);

#endif
