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
	glm_rotate(model_matrix, view->rotation[0], (vec3){ 1, 0, 0 });
	glm_rotate(model_matrix, view->rotation[1], (vec3){ 0, 1, 0 });
	glm_rotate(model_matrix, view->rotation[2], (vec3){ 0, 0, 1 });
}
