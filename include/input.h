#ifndef _WXRC_INPUT_H
#define _WXRC_INPUT_H

#include <cglm/cglm.h>
#include <openxr/openxr.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wayland-server.h>

struct wxrc_server;

enum wxrc_seatop {
	WXRC_SEATOP_DEFAULT,
	WXRC_SEATOP_MOVE,
	WXRC_SEATOP_RESIZE,
};

struct wxrc_keyboard {
	struct wl_list link;
	struct wxrc_server *server;
	struct wlr_input_device *device;

	struct wl_listener modifiers;
	struct wl_listener key;
};

struct wxrc_pointer {
	struct wl_list link;
	struct wxrc_server *server;
	struct wlr_input_device *device;

	struct wl_listener motion;
	struct wl_listener button;
	struct wl_listener axis;
	struct wl_listener frame;
};

struct wxrc_cursor {
	struct wxrc_server *server;
	mat4 matrix;

	struct wlr_xcursor_image *xcursor_image;
	struct wlr_texture *xcursor_texture;

	struct wlr_surface *surface;
	int hotspot_x, hotspot_y;
	struct wl_listener surface_destroy;
};

void wxrc_input_init(struct wxrc_server *server);
void wxrc_update_pointer(struct wxrc_server *server, XrView *xr_view,
	uint32_t time);

void wxrc_cursor_set_xcursor(struct wxrc_cursor *cursor,
	struct wlr_xcursor *xcursor);
void wxrc_cursor_set_surface(struct wxrc_cursor *cursor,
	struct wlr_surface *surface, int hotspot_x, int hotspot_y);
struct wlr_texture *wxrc_cursor_get_texture(struct wxrc_cursor *cursor,
	int *hotspot_x, int *hotspot_y, int *scale);

#endif
