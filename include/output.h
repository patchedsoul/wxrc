#ifndef _WXRC_OUTPUT_H
#define _WXRC_OUTPUT_H

#include <wayland-server-core.h>

struct wxrc_output {
	struct wlr_output *output;
	struct wxrc_server *server;

	struct wl_listener frame;
	struct wl_listener destroy;
};

#endif
