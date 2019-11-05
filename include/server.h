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
	struct wlr_xdg_shell *xdg_shell;
	struct wxrc_gl gl;
};

#endif
