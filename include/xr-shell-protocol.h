#ifndef _WXRC_XR_SHELL_H
#define _WXRC_XR_SHELL_H
#include <cglm/cglm.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>

struct wxrc_zxr_view_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	struct wl_listener display_destroy;
};

struct wxrc_zxr_shell_v1 {
	struct wl_global *global;
	struct wl_list surfaces; // wxrc_zxr_surface_v1.link
	struct wlr_renderer *renderer; // TODO: xr_compositor?

	struct {
		struct wl_signal new_surface; // struct wxrc_zxr_surface_v1 *
		struct wl_signal destroy;
	} events;

	struct wl_listener display_destroy;
};

struct wxrc_zxr_surface_v1 {
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wl_list link; // wxrc_zxr_shell_v1.surfaces
	struct wl_list surface_views; // wxrc_zxr_surface_view_v1.link

	struct {
		struct wl_signal destroy;
	} events;

	struct wl_listener surface_destroy;
};

struct wxrc_zxr_surface_view_v1 {
	struct wl_resource *resource;
	struct wl_list link; // wxrc_zxr_surface_v1.surface_views
	struct wxrc_zxr_surface_v1 *surface;
	struct wxrc_zxr_view_v1 *view;
};

struct wxrc_zxr_composite_buffer_v1 {
	struct wxrc_zxr_shell_v1 *shell;

	struct wl_resource *resource;
	struct wl_resource *buffer_resource;

	struct wl_list buffers; // wxrc_zxr_composite_buffer_v1_view_buffer.link
};

struct wxrc_zxr_composite_buffer_v1_view_buffer {
	struct wxrc_zxr_view_v1 *view;

	struct wl_resource *resource;
	struct wlr_buffer *buffer; // Unset unless wlr_buffer was created for us

	struct wl_list link; // wxrc_zxr_composite_buffer_v1.buffers
};

/**
 * Creates an XR view.
 */
struct wxrc_zxr_view_v1 *wxrc_zxr_view_v1_create(struct wl_display *display);

/**
 * Destroys this XR view.
 */
void wxrc_zxr_view_v1_destroy(struct wxrc_zxr_view_v1 *view);

/**
 * Creates an XR shell.
 */
struct wxrc_zxr_shell_v1 *wxrc_zxr_shell_v1_create(
		struct wlr_renderer *renderer, struct wl_display *display);

/**
 * Destroys this XR shell.
 */
void wxrc_zxr_shell_v1_destroy(struct wxrc_zxr_shell_v1 *shell);

/**
 * Updates the MVP matrix for the specified surface from the specified view's
 * perspective.
 */
void wxrc_zxr_surface_v1_send_mvp_matrix_for_view(
		struct wxrc_zxr_surface_v1 *surface,
		struct wxrc_zxr_view_v1 *view,
		mat4 matrix);

/**
 * Returns true if the specified buffer is a zxr_composite_buffer_v1.
 */
bool wxrc_wlr_buffer_is_zxr_composite_buffer_v1(struct wlr_buffer *buffer);

/**
 * Returns the wxrc_composite_buffer_v1 for this wlr_buffer reference. It is a
 * programming error to call this with a non-composite buffer reference.
 */
struct wxrc_zxr_composite_buffer_v1 *wxrc_zxr_composite_buffer_v1_from_buffer(
		struct wlr_buffer *buffer);

/**
 * Returns a wlr_texture for a particular view, or NULL if no buffer is provided
 * for this view.
 */
struct wlr_texture *wxrc_zxr_composite_buffer_v1_for_view(
		struct wxrc_zxr_composite_buffer_v1 *buffer,
		struct wxrc_zxr_view_v1 *view);

#endif
