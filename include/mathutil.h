#ifndef _WXRC_MATHUTIL_H
#define _WXRC_MATHUTIL_H

#include <cglm/cglm.h>

struct wlr_surface;

bool wxrc_intersect_plane_line(vec3 plane_point, vec3 plane_normal,
	vec3 line_point, vec3 line_dir, vec3 intersection);
bool wxrc_intersect_surface_line(struct wlr_surface *surface,
	vec3 surface_position, vec3 surface_rotation, vec3 line_position,
	vec3 line_dir, vec3 intersection, float *sx_ptr, float *sy_ptr);

#endif
