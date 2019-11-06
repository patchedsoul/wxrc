#ifndef WXRC_VIEW_H
#define WXRC_VIEW_H
#include <cglm/cglm.h>
#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/config.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_xdg_shell.h>
#if WLR_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif
#include "render.h"

struct wxrc_server;

enum wxrc_view_type {
	WXRC_VIEW_XDG_SHELL,
	WXRC_VIEW_XWAYLAND,
};

struct wxrc_view {
	enum wxrc_view_type view_type;
	struct wxrc_server *server;
	struct wlr_surface *surface;

	vec3 position, rotation;
	bool mapped;

	struct wl_list link;
};

struct wxrc_xdg_shell_view {
	struct wxrc_view base;

	struct wlr_xdg_surface *xdg_surface;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
};

void wxrc_xdg_shell_init(struct wxrc_server *server);

#endif