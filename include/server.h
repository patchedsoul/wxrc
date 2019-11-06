#ifndef WXRC_SERVER_H
#define WXRC_SERVER_H

#include <wayland-server.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "render.h"

struct wxrc_xr_backend;

struct wxrc_toplevel {
	/* TODO: other types of toplevels (e.g. xwayland) */
	struct wlr_xdg_surface *xdg_surface;
	struct wl_list link;

	struct wl_listener xdg_surface_destroy;
};

struct wxrc_server {
	struct wl_display *wl_display;

	struct wxrc_xr_backend *backend;
	struct wxrc_gl gl;

	struct wlr_compositor *compositor;
	struct wlr_xdg_shell *xdg_shell;

	struct wl_list toplevels;

	struct wl_listener new_xdg_surface;
};

#endif
