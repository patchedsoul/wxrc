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
#include "xr-shell-protocol.h"
#include "render.h"

struct wxrc_server;

struct wxrc_view;

struct wxrc_view_interface {
	void (*for_each_surface)(struct wxrc_view *view,
		wlr_surface_iterator_func_t iterator, void *user_data);
	struct wlr_surface *(*surface_at)(struct wxrc_view *view,
		double sx, double sy, double *child_sx, double *child_sy);
	void (*set_activated)(struct wxrc_view *view, bool activated);
	void (*close)(struct wxrc_view *view);
	void (*get_size)(struct wxrc_view *view, int *width, int *height);
	void (*set_size)(struct wxrc_view *view, int width, int height);
};

struct wxrc_view {
	struct wxrc_server *server;
	const struct wxrc_view_interface *impl;
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

struct wxrc_zxr_shell_view {
	struct wxrc_view base;

	struct wxrc_zxr_surface_v1 *xr_surface;

	struct wl_listener destroy;
};

void wxrc_xdg_shell_init(struct wxrc_server *server);
void wxrc_xr_shell_init(struct wxrc_server *server,
		struct wlr_renderer *renderer);

void wxrc_view_init(struct wxrc_view *view, struct wxrc_server *server,
	const struct wxrc_view_interface *impl, struct wlr_surface *surface);
void wxrc_view_finish(struct wxrc_view *view);
void wxrc_view_get_model_matrix(struct wxrc_view *view, mat4 matrix);
void wxrc_view_get_2d_model_matrix(struct wxrc_view *view,
	struct wlr_surface *surface, int sx, int sy, mat4 model_matrix);
struct wxrc_view *wxrc_get_focus(struct wxrc_server *server);
void wxrc_set_focus(struct wxrc_view *view);
void wxrc_view_begin_move(struct wxrc_view *view);
void wxrc_view_close(struct wxrc_view *view);

/* Sets to zero if surface is not two dimensional */
/* TODO: 3D resize? */
void wxrc_view_get_size(struct wxrc_view *view, int *width, int *height);
void wxrc_view_set_size(struct wxrc_view *view, int width, int height);

void wxrc_view_for_each_surface(struct wxrc_view *view,
	wlr_surface_iterator_func_t iterator, void *user_data);
struct wlr_surface *wxrc_view_surface_at(struct wxrc_view *view,
	double sx, double sy, double *child_sx, double *child_sy);

bool wxrc_view_is_xr_shell(struct wxrc_view *view);

#endif
