#ifndef _WXRC_INPUT_H
#define _WXRC_INPUT_H

#include <openxr/openxr.h>
#include <wlr/types/wlr_input_device.h>
#include <wayland-server.h>

struct wxrc_server;

struct wxrc_keyboard {
	struct wl_list link;
	struct wxrc_server *server;
	struct wlr_input_device *device;

	struct wl_listener modifiers;
	struct wl_listener key;
};

void wxrc_input_init(struct wxrc_server *server);
void wxrc_update_pointer(struct wxrc_server *server, XrView *xr_view,
	uint32_t time);

#endif
