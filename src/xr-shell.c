#include "server.h"
#include "view.h"
#include "xr-shell-protocol.h"

static void handle_xr_surface_destroy(struct wl_listener *listener,
		void *data) {
	struct wxrc_xr_shell_view *xr_view =
		wl_container_of(listener, xr_view, destroy);
	wl_list_remove(&xr_view->destroy.link);
	wxrc_view_finish(&xr_view->base);
	free(xr_view);
}

static void handle_new_xr_surface(struct wl_listener *listener, void *data) {
	struct wxrc_server *server =
		wl_container_of(listener, server, new_xr_surface);
	struct wxrc_xr_surface_v1 *xr_surface = data;

	struct wxrc_xr_shell_view *xr_view = calloc(1, sizeof(*xr_view));
	wxrc_view_init(&xr_view->base, server, WXRC_VIEW_XR_SHELL,
		xr_surface->surface);
	xr_view->xr_surface = xr_surface;

	xr_view->destroy.notify = handle_xr_surface_destroy;
	wl_signal_add(&xr_surface->events.destroy, &xr_view->destroy);

	xr_view->base.mapped = true;
	xr_view->base.position[0] = 0.0;
	xr_view->base.position[1] = 0.0;
	xr_view->base.position[2] = -1.0;
}

void wxrc_xr_shell_init(struct wxrc_server *server) {
	server->xr_shell = wxrc_xr_shell_v1_create(server->wl_display);

	server->new_xr_surface.notify = handle_new_xr_surface;
	wl_signal_add(&server->xr_shell->events.new_surface,
			&server->new_xr_surface);
}
