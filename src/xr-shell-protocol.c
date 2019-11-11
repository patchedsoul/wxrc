#include <stdlib.h>
#include <wlr/types/wlr_surface.h>
#include "wxrc-xr-shell-unstable-v1-protocol.h"
#include "xr-shell-protocol.h"

static struct wxrc_xr_surface_v1 *surface_from_resource(
		struct wl_resource *resource) {
	// TODO: assert
	return wl_resource_get_user_data(resource);
}

static struct wxrc_xr_shell_v1 *shell_from_resource(
		struct wl_resource *resource) {
	// TODO: assert
	return wl_resource_get_user_data(resource);
}

void wxrc_xr_surface_v1_send_matrix(struct wxrc_xr_surface_v1 *xr_surface,
		mat4 matrix) {
	struct wl_array matrix_array = {
		.size = sizeof(mat4),
		.data = matrix,
	};
	zwxrc_xr_surface_v1_send_mvp_matrix(xr_surface->resource, &matrix_array);
}

static void surface_handle_resource_destroy(struct wl_resource *resource) {
	struct wxrc_xr_surface_v1 *xr_surface = surface_from_resource(resource);
	wl_signal_emit(&xr_surface->events.destroy, xr_surface);
	wl_list_remove(&xr_surface->link);
	free(xr_surface);
}

static void shell_handle_get_xr_surface(struct wl_client *client,
		struct wl_resource *resource, uint32_t id,
		struct wl_resource *surface_resource) {
	struct wxrc_xr_shell_v1 *shell = shell_from_resource(resource);

	struct wxrc_xr_surface_v1 *xr_surface = calloc(1, sizeof(*xr_surface));
	if (xr_surface == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	xr_surface->surface = wlr_surface_from_resource(surface_resource);

	wl_signal_init(&xr_surface->events.destroy);

	uint32_t version = wl_resource_get_version(resource);
	xr_surface->resource = wl_resource_create(client,
		&zwxrc_xr_surface_v1_interface, version, id);
	if (xr_surface->resource == NULL) {
		free(xr_surface);
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(xr_surface->resource, NULL,
		xr_surface, surface_handle_resource_destroy);

	wl_list_insert(&shell->surfaces, &xr_surface->link);

	// TODO: surface destroy listener

	wl_signal_emit(&shell->events.new_surface, xr_surface);
}

static const struct zwxrc_xr_shell_v1_interface shell_impl = {
	.get_xr_surface = shell_handle_get_xr_surface,
};

static void shell_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wxrc_shell_v1 *shell = data;

	struct wl_resource *resource = wl_resource_create(client,
		&zwxrc_xr_shell_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &shell_impl, shell, NULL);
}

static void shell_handle_display_destroy(struct wl_listener *listener,
		void *data) {
	struct wxrc_xr_shell_v1 *shell =
		wl_container_of(listener, shell, display_destroy);
	wl_signal_emit(&shell->events.destroy, shell);
	wl_list_remove(&shell->display_destroy.link);
	wl_global_destroy(shell->global);
	free(shell);
}

struct wxrc_xr_shell_v1 *wxrc_xr_shell_v1_create(struct wl_display *display) {
	struct wxrc_xr_shell_v1 *shell = calloc(1, sizeof(*shell));
	if (shell == NULL) {
		return NULL;
	}

	shell->global = wl_global_create(display, &zwxrc_xr_shell_v1_interface, 1,
		shell, shell_bind);
	if (shell->global == NULL) {
		free(shell);
		return NULL;
	}

	wl_list_init(&shell->surfaces);
	wl_signal_init(&shell->events.new_surface);
	wl_signal_init(&shell->events.destroy);

	shell->display_destroy.notify = shell_handle_display_destroy;
	wl_display_add_destroy_listener(display, &shell->display_destroy);

	return shell;
}
