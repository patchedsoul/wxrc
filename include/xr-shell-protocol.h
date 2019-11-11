#ifndef _WXRC_XR_SHELL_H
#define _WXRC_XR_SHELL_H

#include <wayland-server-core.h>
#include <cglm/cglm.h>

struct wxrc_xr_surface_v1 {
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wl_list link; // wxrc_xr_shell_v1.surfaces

	struct {
		struct wl_signal destroy;
	} events;

	struct wl_listener surface_destroy;
};

struct wxrc_xr_shell_v1 {
	struct wl_global *global;
	struct wl_list surfaces; // wxrc_xr_surface_v1.link

	struct {
		struct wl_signal new_surface; // struct wxrc_xr_surface_v1 *
		struct wl_signal destroy;
	} events;

	struct wl_listener display_destroy;
};

struct wxrc_xr_shell_v1 *wxrc_xr_shell_v1_create(struct wl_display *display);
void wxrc_xr_surface_v1_send_matrix(struct wxrc_xr_surface_v1 *xr_surface,
	mat4 matrix);

#endif
