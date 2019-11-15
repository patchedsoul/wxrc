#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include "input.h"
#include "server.h"
#include "view.h"
#include "xrutil.h"

static const struct wxrc_view_interface xdg_shell_view_impl;

static struct wxrc_xdg_shell_view *xdg_shell_view_from_view(
		struct wxrc_view *view) {
	assert(view->impl == &xdg_shell_view_impl);
	return (struct wxrc_xdg_shell_view *)view;
}

static void set_activated(struct wxrc_view *view, bool activated) {
	struct wxrc_xdg_shell_view *xdg_view = xdg_shell_view_from_view(view);
	wlr_xdg_toplevel_set_activated(xdg_view->xdg_surface, activated);
}

static void close(struct wxrc_view *view) {
	struct wxrc_xdg_shell_view *xdg_view = xdg_shell_view_from_view(view);
	wlr_xdg_toplevel_send_close(xdg_view->xdg_surface);
}

static const struct wxrc_view_interface xdg_shell_view_impl = {
	.set_activated = set_activated,
	.close = close,
};

static void handle_xdg_surface_map(struct wl_listener *listener, void *data) {
	struct wxrc_xdg_shell_view *view = wl_container_of(listener, view, map);

	XrView *xr_view = &view->base.server->xr_views[0];

	mat4 view_matrix;
	wxrc_xr_view_get_matrix(xr_view, view_matrix);

	vec3 pos = { 0.0, 0.0, -2.0 };
	glm_vec3_rotate_m4(view_matrix, pos, pos);

	vec3 rot;
	glm_euler_angles(view_matrix, rot);

	glm_vec3_copy(pos, view->base.position);
	glm_vec3_copy(rot, view->base.rotation);

	wlr_log(WLR_DEBUG, "Spawning view at <%f,%f,%f>", pos[0], pos[1], pos[2]);

	wxrc_set_focus(&view->base);
	view->base.mapped = true;
}

static void handle_xdg_surface_unmap(struct wl_listener *listener, void *data) {
	struct wxrc_xdg_shell_view *view = wl_container_of(listener, view, unmap);
	view->base.mapped = false;

	struct wxrc_view *wview;
	wl_list_for_each(wview, &view->base.server->views, link) {
		if (wview->mapped) {
			wxrc_set_focus(wview);
			break;
		}
	}
}

static void handle_xdg_surface_request_move(
		struct wl_listener *listener, void *data) {
	struct wxrc_xdg_shell_view *view =
		wl_container_of(listener, view, request_move);
	wxrc_view_begin_move(&view->base);
}

static void handle_xdg_surface_destroy(
		struct wl_listener *listener, void *data) {
	struct wxrc_xdg_shell_view *view = wl_container_of(listener, view, destroy);
	wxrc_view_finish(&view->base);
	free(view);
}

static void handle_new_xdg_surface(struct wl_listener *listener, void *data) {
	struct wxrc_server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}

	struct wxrc_xdg_shell_view *view =
		calloc(1, sizeof(struct wxrc_xdg_shell_view));
	wxrc_view_init(&view->base, server, WXRC_VIEW_XDG_SHELL,
		&xdg_shell_view_impl, xdg_surface->surface);
	view->xdg_surface = xdg_surface;

	view->map.notify = handle_xdg_surface_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = handle_xdg_surface_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->request_move.notify = handle_xdg_surface_request_move;
	wl_signal_add(&xdg_surface->toplevel->events.request_move,
		&view->request_move);
	view->destroy.notify = handle_xdg_surface_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);
}

void wxrc_xdg_shell_init(struct wxrc_server *server) {
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display);
	server->new_xdg_surface.notify = handle_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface,
		&server->new_xdg_surface);
}
