#include <wlr/types/wlr_surface.h>
#include "mathutil.h"
#include "render.h"

void wxrc_mat4_rotate(mat4 m, vec3 angles) {
	glm_rotate(m, angles[0], (vec3){ 1, 0, 0 });
	glm_rotate(m, angles[1], (vec3){ 0, 1, 0 });
	glm_rotate(m, angles[2], (vec3){ 0, 0, 1 });
}

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

static void vec3_rotate(vec3 angles, vec3 vec) {
	// Note: don't apply rotations axis by axis, this doesn't work because it's
	// not atomic.
	mat4 m = GLM_MAT4_IDENTITY_INIT;
	wxrc_mat4_rotate(m, angles);
	glm_vec3_rotate_m4(m, vec, vec);
}

bool wxrc_intersect_surface_plane_line(struct wlr_surface *surface,
		mat4 model_matrix, vec3 surface_position, vec3 surface_rotation,
		vec3 line_position, vec3 line_dir, vec3 intersection,
		float *sx_ptr, float *sy_ptr) {
	vec3 surface_normal = { 0.0, 0.0, -1.0 };
	vec3_rotate(surface_rotation, surface_normal);

	if (!wxrc_intersect_plane_line(surface_position, surface_normal,
			line_position, line_dir, intersection)) {
		return false;
	}

	// Transform world coords into model coords
	vec4 pos = { intersection[0], intersection[1], intersection[2], 1.0 };
	mat4 inv_model_matrix;
	glm_mat4_inv(model_matrix, inv_model_matrix);
	glm_mat4_mulv(inv_model_matrix, pos, pos);

	float width = surface->current.buffer_width;
	float height = surface->current.buffer_height;

	float sx = pos[0] * width;
	float sy = (1.0 - pos[1]) * height;

	*sx_ptr = sx;
	*sy_ptr = sy;
	return true;
}
