#include <wlr/types/wlr_surface.h>
#include "mathutil.h"
#include "render.h"

bool wxrc_intersect_plane_line(vec3 plane_point, vec3 plane_normal,
		vec3 line_point, vec3 line_dir, vec3 intersection) {
	vec3 pt_delta;
	glm_vec3_sub(plane_point, line_point, pt_delta);

	float denom = glm_vec3_dot(plane_normal, line_dir);
	if (fabs(denom) <= 0.0001) {
		return false; // parallel
	}

	float t = glm_vec3_dot(plane_normal, pt_delta) / denom;
	if (t <= 0) {
		return false; // wrong direction
	}

	intersection[0] = line_dir[0] * t + line_point[0];
	intersection[1] = line_dir[1] * t + line_point[1];
	intersection[2] = line_dir[2] * t + line_point[2];

	return true;
}

static void rotate_vec3_with_angles(vec3 angles, vec3 vec) {
	glm_vec3_rotate(vec, angles[0], (vec3){ 1, 0, 0 });
	glm_vec3_rotate(vec, angles[1], (vec3){ 0, 1, 0 });
	glm_vec3_rotate(vec, -angles[2], (vec3){ 0, 0, 1 });
}

bool wxrc_intersect_surface_line(struct wlr_surface *surface,
		vec3 surface_position, vec3 surface_rotation, vec3 line_position,
		vec3 line_dir, vec3 intersection, float *sx_ptr, float *sy_ptr) {
	vec3 surface_normal = { 0.0, 0.0, -1.0 };
	rotate_vec3_with_angles(surface_rotation, surface_normal);

	if (!wxrc_intersect_plane_line(surface_position, surface_normal,
			line_position, line_dir, intersection)) {
		return false;
	}

	vec3 delta;
	glm_vec3_sub(intersection, surface_position, delta);

	vec3 x_vec = { 1.0, 0.0, 0.0 };
	rotate_vec3_with_angles(surface_rotation, x_vec);

	vec3 y_vec = { 0.0, 1.0, 0.0 };
	rotate_vec3_with_angles(surface_rotation, y_vec);

	// Project delta in surface coordinate system
	float x = glm_vec3_dot(x_vec, delta);
	float y = -glm_vec3_dot(y_vec, delta);

	// Scale and re-center surface coordinates
	float width = surface->current.width;
	float height = surface->current.height;
	float sx = x * WXRC_SURFACE_SCALE + 0.5 * width;
	float sy = y * WXRC_SURFACE_SCALE + 0.5 * height;

	if (sx >= 0 && sy >= 0 && sx < width && sy < height) {
		*sx_ptr = sx;
		*sy_ptr = sy;
		return true;
	} else {
		return false;
	}
}
