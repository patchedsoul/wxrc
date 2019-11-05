#ifndef WXRC_SERVER_H
#define WXRC_SERVER_H

#include <wayland-server.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "render.h"

struct wxrc_xr_backend;

struct wxrc_server {
	struct wl_display *wl_display;

	struct wxrc_xr_backend *backend;
	struct wxrc_gl gl;

	struct wlr_compositor *compositor;
	struct wlr_xdg_shell *xdg_shell;

	struct wlr_surface *current_surface;

	struct wl_listener new_surface;
};

#endif
