#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include "input.h"
#include "server.h"
#include "view.h"
#include "xrutil.h"

static void handle_xdg_surface_map(struct wl_listener *listener, void *data) {
	struct wxrc_xdg_shell_view *view = wl_container_of(listener, view, map);
	vec3 pos = { 0.0, 0.0, -2.0 };
	vec3 rot = { 0.0, 0.0, 0.0 };

	/* TODO: Move this into shared code */
	mat4 view_matrix;
	versor orientation;
	vec3 position;

	XrView *xr_view = &view->base.server->xr_views[0];
	wxrc_xr_vector3f_to_cglm(&xr_view->pose.position, position);
	position[1] = 0; /* TODO: don't zero out Y-axis */
	wxrc_xr_quaternion_to_cglm(&xr_view->pose.orientation, orientation);

	glm_quat_mat4(orientation, view_matrix);
	glm_translate(view_matrix, position);
	glm_vec3_rotate_m4(view_matrix, pos, pos);

	glm_euler_angles(view_matrix, rot);

	glm_vec3_copy(pos, view->base.position);
	glm_vec3_copy(rot, view->base.rotation);

	wlr_log(WLR_DEBUG, "Spawning view at <%f,%f,%f>",
			pos[0], pos[1], pos[2]);

	focus_view(&view->base, NULL);
	view->base.mapped = true;
}

static void handle_xdg_surface_unmap(struct wl_listener *listener, void *data) {
	struct wxrc_xdg_shell_view *view = wl_container_of(listener, view, unmap);
	view->base.mapped = false;

	struct wxrc_view *wview;
	wl_list_for_each(wview, &view->base.server->views, link) {
		if (wview->mapped) {
			focus_view(wview, wview->surface);
			break;
		}
	}
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
		xdg_surface->surface);
	view->xdg_surface = xdg_surface;

	view->map.notify = handle_xdg_surface_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = handle_xdg_surface_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = handle_xdg_surface_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);
}

void wxrc_xdg_shell_init(struct wxrc_server *server) {
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display);
	server->new_xdg_surface.notify = handle_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface,
			&server->new_xdg_surface);
}
