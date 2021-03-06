#ifndef WXRC_SERVER_H
#define WXRC_SERVER_H
#include <cglm/cglm.h>
#include <openxr/openxr.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "input.h"
#include "render.h"
#include "xr-shell-protocol.h"

struct wxrc_xr_backend;

struct wxrc_server {
	struct wl_display *wl_display;

	struct wlr_backend *backend;
	struct wxrc_xr_backend *xr_backend;
	struct wxrc_gl gl;

	XrView *xr_views;

	struct wlr_compositor *compositor;
	struct wlr_xdg_shell *xdg_shell;
	struct wxrc_zxr_shell_v1 *xr_shell;

	struct wl_seat *remote_seat;
	struct wl_pointer *remote_pointer;
	struct zwp_pointer_constraints_v1 *remote_pointer_constraints;

	struct wl_list views;

	struct wlr_seat *seat;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wxrc_cursor cursor;
	vec3 pointer_rotation; /* relative to the first XR view's orientation */
	enum wxrc_seatop seatop;
	float seatop_sx, seatop_sy;
	int seatop_w, seatop_h;

	struct wl_list keyboards;
	struct wl_list pointers;

	struct wl_listener new_input;
	struct wl_listener new_output;
	struct wl_listener new_xdg_surface;
	struct wl_listener new_xr_surface;
	struct wl_listener request_set_cursor;
	struct wl_listener request_set_selection;
	struct wl_listener request_set_primary_selection;
};

#endif
