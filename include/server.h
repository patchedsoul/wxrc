#ifndef WXRC_SERVER_H
#define WXRC_SERVER_H

#include <wayland-server.h>

struct wxrc_xr_backend;

struct wxrc_server {
	struct wl_display *wl_display;
	struct wxrc_xr_backend *backend;
};

#endif
