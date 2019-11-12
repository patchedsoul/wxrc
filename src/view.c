#include "mathutil.h"
#include "render.h"
#include "server.h"
#include "view.h"

void wxrc_view_init(struct wxrc_view *view, struct wxrc_server *server,
		enum wxrc_view_type type, struct wlr_surface *surface) {
	view->server = server;
	view->view_type = type;
	view->surface = surface;

	wl_list_insert(&server->views, &view->link);
}

void wxrc_view_finish(struct wxrc_view *view) {
	wl_list_remove(&view->link);
}

void wxrc_view_get_model_matrix(struct wxrc_view *view, mat4 model_matrix) {
	glm_mat4_identity(model_matrix);

	glm_translate(model_matrix, view->position);
	wxrc_mat4_rotate(model_matrix, view->rotation);
}

void wxrc_view_get_2d_model_matrix(struct wxrc_view *view, mat4 model_matrix) {
	int width = view->surface->current.buffer_width;
	int height = view->surface->current.buffer_height;

	float scale_x = width / WXRC_SURFACE_SCALE;
	float scale_y = height / WXRC_SURFACE_SCALE;

	wxrc_view_get_model_matrix(view, model_matrix);

	glm_scale(model_matrix, (vec3){ scale_x, scale_y, 1.0 });

	/* Re-origin the view to the center */
	glm_translate(model_matrix, (vec3){ -0.5, -0.5, 0.0 });
}
