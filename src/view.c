#include "mathutil.h"
#include "render.h"
#include "server.h"
#include "view.h"

void wxrc_view_init(struct wxrc_view *view, struct wxrc_server *server,
		const struct wxrc_view_interface *impl, struct wlr_surface *surface) {
	view->server = server;
	view->impl = impl;
	view->surface = surface;

	wl_list_insert(server->views.prev, &view->link);
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

struct wxrc_view *wxrc_get_focus(struct wxrc_server *server) {
	if (wl_list_empty(&server->views)) {
		return NULL;
	}
	struct wxrc_view *view = wl_container_of(server->views.next, view, link);
	if (!view->mapped) {
		return NULL;
	}
	return view;
}

void wxrc_set_focus(struct wxrc_view *view) {
	if (view == NULL) {
		return;
	}

	struct wxrc_server *server = view->server;

	struct wxrc_view *prev_view = wxrc_get_focus(server);
	if (prev_view == view) {
		return;
	}
	if (prev_view != NULL && prev_view->impl->set_activated) {
		prev_view->impl->set_activated(prev_view, false);
	}

	wl_list_remove(&view->link);
	wl_list_insert(&server->views, &view->link);

	struct wlr_seat *seat = server->seat;
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	wlr_seat_keyboard_notify_enter(seat, view->surface,
		keyboard->keycodes, keyboard->num_keycodes,
		&keyboard->modifiers);

	if (view->impl->set_activated) {
		view->impl->set_activated(view, true);
	}
}

void wxrc_view_begin_move(struct wxrc_view *view) {
	struct wxrc_server *server = view->server;
	if (wxrc_get_focus(server) != view ||
			server->seatop != WXRC_SEATOP_DEFAULT) {
		return;
	}
	wlr_seat_pointer_clear_focus(server->seat);
	server->seatop = WXRC_SEATOP_MOVE;
}

void wxrc_view_close(struct wxrc_view *view) {
	if (view->impl->close) {
		view->impl->close(view);
	}
}
