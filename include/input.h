#ifndef _WXRC_INPUT_H
#define _WXRC_INPUT_H

struct wxrc_server;

struct wxrc_keyboard {
	struct wl_list link;
	struct wxrc_server *server;
	struct wlr_input_device *device;

	struct wl_listener modifiers;
	struct wl_listener key;
};

void wxrc_input_init(struct wxrc_server *server);

#endif
