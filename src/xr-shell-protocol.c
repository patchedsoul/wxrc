#include <stdlib.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/util/log.h>
#include "zxr-shell-unstable-v1-protocol.h"
#include "xr-shell-protocol.h"

static struct wxrc_zxr_view_v1 *view_from_resource(
		struct wl_resource *resource) {
	// TODO: assert
	return wl_resource_get_user_data(resource);
}

static struct wxrc_zxr_shell_v1 *shell_from_resource(
		struct wl_resource *resource) {
	// TODO: assert
	return wl_resource_get_user_data(resource);
}

static struct wxrc_zxr_surface_v1 *surface_from_resource(
		struct wl_resource *resource) {
	// TODO: assert
	return wl_resource_get_user_data(resource);
}

static struct wxrc_zxr_surface_view_v1 *surface_view_from_resource(
		struct wl_resource *resource) {
	// TODO: assert
	return wl_resource_get_user_data(resource);
}

static struct wxrc_zxr_composite_buffer_v1 *composite_buffer_from_resource(
		struct wl_resource *resource) {
	// TODO: assert
	return wl_resource_get_user_data(resource);
}

static void composite_buffer_handle_destroy(struct wl_client *client,
		struct wl_resource *buffer) {
	wl_resource_destroy(buffer);
}

struct wxrc_zxr_composite_buffer_v1 *wxrc_zxr_composite_buffer_v1_from_buffer(
		struct wlr_buffer *buffer) {
	return composite_buffer_from_resource(buffer->resource);
}

struct wlr_texture *wxrc_zxr_composite_buffer_v1_for_view(
		struct wxrc_zxr_composite_buffer_v1 *buffer,
		struct wxrc_zxr_view_v1 *view,
		enum zxr_composite_buffer_v1_buffer_type buffer_type) {
	struct wxrc_zxr_composite_buffer_v1_view_buffer *vb;
	wl_list_for_each(vb, &buffer->buffers, link) {
		if (vb->view == view
				&& vb->buffer_type == buffer_type
				&& vb->buffer != NULL) {
			return vb->buffer->texture;
		}
	}
	return NULL;
}

static const struct wl_buffer_interface composite_wl_buffer_impl = {
	.destroy = composite_buffer_handle_destroy,
};

static bool composite_buffer_is_instance(struct wl_resource *buffer_resource) {
	if (!wl_resource_instance_of(buffer_resource,
				&wl_buffer_interface, &composite_wl_buffer_impl)) {
		return false;
	}
	struct wxrc_zxr_composite_buffer_v1 *buffer =
		wl_resource_get_user_data(buffer_resource);
	return buffer && buffer->resource;
}

static bool composite_buffer_initialize(struct wlr_buffer *buffer,
		struct wl_resource *resource, struct wlr_renderer *renderer) {
	struct wxrc_zxr_composite_buffer_v1 *cbuffer =
		composite_buffer_from_resource(resource);

	bool success = true;

	struct wxrc_zxr_composite_buffer_v1_view_buffer *vb;
	wl_list_for_each(vb, &cbuffer->buffers, link) {
		/* TODO: Is this necessary?
		wlr_buffer_unref(vb->buffer);
		vb->buffer = NULL;
		*/
		vb->buffer = wlr_buffer_create(renderer, vb->resource);
		success = success && vb->buffer != NULL;
	}

	return success;
}

static bool composite_buffer_get_resource_size(struct wl_resource *resource,
		struct wlr_renderer *renderer, int *width, int *height) {
	struct wxrc_zxr_composite_buffer_v1 *cbuffer =
		composite_buffer_from_resource(resource);

	struct wxrc_zxr_composite_buffer_v1_view_buffer *vb;
	wl_list_for_each(vb, &cbuffer->buffers, link) {
		/* TODO: Something better I guess */
		return wlr_buffer_get_resource_size(
				vb->resource, renderer, width, height);
	}

	return false;
}

static void composite_buffer_destroy(struct wlr_buffer *buffer) {
	struct wxrc_zxr_composite_buffer_v1 *cbuffer =
		composite_buffer_from_resource(buffer->resource);

	struct wxrc_zxr_composite_buffer_v1_view_buffer *vb;
	wl_list_for_each(vb, &cbuffer->buffers, link) {
		wlr_buffer_unref(vb->buffer);
		vb->buffer = NULL;
	}
}

static const struct wlr_buffer_impl composite_wlr_buffer_impl = {
	.is_instance = composite_buffer_is_instance,
	.initialize = composite_buffer_initialize,
	.get_resource_size = composite_buffer_get_resource_size,
	.destroy = composite_buffer_destroy,
	/* TODO: Do we want apply_damage? If our view buffers are wl_shm it might
	 * matter */
};

static void composite_buffer_handle_attach_buffer(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *view_resource,
		struct wl_resource *buffer, uint32_t buffer_type) {
	struct wxrc_zxr_composite_buffer_v1 *cbuffer =
		composite_buffer_from_resource(resource);
	struct wxrc_zxr_view_v1 *view = view_from_resource(view_resource);

	if (buffer_type != ZXR_COMPOSITE_BUFFER_V1_BUFFER_TYPE_PIXEL_BUFFER
			&& buffer_type != ZXR_COMPOSITE_BUFFER_V1_BUFFER_TYPE_DEPTH_BUFFER) {
		wl_resource_post_error(resource,
				ZXR_COMPOSITE_BUFFER_V1_ERROR_INVALID_BUFFER,
				"Unknown buffer type %d", buffer_type);
		return;
	}

	struct wxrc_zxr_composite_buffer_v1_view_buffer *_view_buffer,
		*view_buffer = NULL;
	wl_list_for_each(_view_buffer, &cbuffer->buffers, link) {
		if (_view_buffer->view == view
				&& _view_buffer->buffer_type == buffer_type) {
			view_buffer = _view_buffer;
			break;
		}
	}

	if (buffer == NULL) { /* Removing buffer for view */
		if (view_buffer && view_buffer->buffer) {
			wlr_buffer_unref(view_buffer->buffer);
			view_buffer->buffer = NULL;
		}
		if (view_buffer == NULL) {
			return;
		}
		wl_list_remove(&view_buffer->link);
		free(view_buffer);
		return;
	}

	if (view_buffer == NULL) {
		view_buffer = calloc(1,
				sizeof(struct wxrc_zxr_composite_buffer_v1_view_buffer));
		view_buffer->view = view;
		view_buffer->buffer_type = buffer_type;
		wl_list_insert(&cbuffer->buffers, &view_buffer->link);
	}

	if (view_buffer->buffer) {
		wlr_buffer_unref(view_buffer->buffer);
		view_buffer->buffer = NULL;
	}
	view_buffer->resource = buffer;
}

static void composite_buffer_handle_buffer_resource_destroy(
		struct wl_resource *resource) {
	/* TODO: ??? */
}

static void composite_buffer_handle_get_wl_buffer(struct wl_client *client,
		struct wl_resource *resource, uint32_t buffer_id) {
	struct wxrc_zxr_composite_buffer_v1 *cbuffer =
		composite_buffer_from_resource(resource);

	cbuffer->buffer_resource = wl_resource_create(
			client, &wl_buffer_interface, 1, buffer_id);
	if (!cbuffer->buffer_resource) {
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_resource_set_implementation(cbuffer->buffer_resource,
		&composite_wl_buffer_impl, cbuffer,
		composite_buffer_handle_buffer_resource_destroy);
}

static const struct zxr_composite_buffer_v1_interface composite_buffer_impl = {
	.attach_buffer = composite_buffer_handle_attach_buffer,
	.get_wl_buffer = composite_buffer_handle_get_wl_buffer,
};

static void composite_buffer_handle_resource_destroy(
		struct wl_resource *resource) {
	struct wxrc_zxr_composite_buffer_v1 *buffer =
		composite_buffer_from_resource(resource);
	/* TODO: unref children */
	free(buffer);
}

static void shell_handle_create_composite_buffer(struct wl_client *client,
		struct wl_resource *resource, uint32_t id) {
	struct wxrc_zxr_composite_buffer_v1 *buffer =
		calloc(1, sizeof(struct wxrc_zxr_composite_buffer_v1));
	wl_list_init(&buffer->buffers);

	uint32_t version = wl_resource_get_version(resource);
	buffer->resource = wl_resource_create(client,
			&zxr_composite_buffer_v1_interface, version, id);
	if (buffer->resource == NULL) {
		free(buffer);
		wl_client_post_no_memory(client);
		return;
	}
	buffer->shell = shell_from_resource(resource);
	wl_resource_set_implementation(buffer->resource, &composite_buffer_impl,
			buffer, composite_buffer_handle_resource_destroy);
}

static void handle_view_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	// TODO
}

static const struct zxr_view_v1_interface view_impl = {
	.destroy = handle_view_destroy,
};

static void view_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wxrc_zxr_view_v1 *view = data;

	struct wl_resource *resource = wl_resource_create(client,
		&zxr_view_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &view_impl, view, NULL);
}

static void view_handle_display_destroy(
		struct wl_listener *listener, void *data) {
	struct wxrc_zxr_view_v1 *view =
		wl_container_of(listener, view, display_destroy);
	wl_signal_emit(&view->events.destroy, view);
	wl_list_remove(&view->display_destroy.link);
	wl_global_destroy(view->global);
	free(view);
}

struct wxrc_zxr_view_v1 *wxrc_zxr_view_v1_create(struct wl_display *display) {
	struct wxrc_zxr_view_v1 *view = calloc(1, sizeof(struct wxrc_zxr_view_v1));
	if (view == NULL) {
		return NULL;
	}

	wl_signal_init(&view->events.destroy);

	view->global = wl_global_create(display, &zxr_view_v1_interface, 1,
		view, view_bind);
	if (view->global == NULL) {
		free(view);
		return NULL;
	}

	view->display_destroy.notify = view_handle_display_destroy;
	wl_display_add_destroy_listener(display, &view->display_destroy);
	return view;
}

static void surface_view_handle_resource_destroy(struct wl_resource *resource) {
	struct wxrc_zxr_surface_view_v1 *surface_view =
		surface_view_from_resource(resource);
	wl_list_remove(&surface_view->link);
	free(surface_view);
}

void wxrc_zxr_surface_v1_send_mvp_matrix_for_view(
		struct wxrc_zxr_surface_v1 *surface,
		struct wxrc_zxr_view_v1 *view,
		mat4 matrix) {
	struct wxrc_zxr_surface_view_v1 *surface_view;
	struct wl_array matrix_array = {
		.size = sizeof(mat4),
		.data = matrix,
	};
	wl_list_for_each(surface_view, &surface->surface_views, link) {
		if (surface_view->view == view) {
			zxr_surface_view_v1_send_mvp_matrix(
					surface_view->resource, &matrix_array);
			break;
		}
	}
}

static void handle_surface_get_surface_view(
		struct wl_client *client, struct wl_resource *resource,
		uint32_t surface_view_id, struct wl_resource *view_resource) {
	struct wxrc_zxr_surface_v1 *surface = surface_from_resource(resource);
	struct wxrc_zxr_view_v1 *view = view_from_resource(view_resource);

	struct wxrc_zxr_surface_view_v1 *surface_view =
		calloc(1, sizeof(struct wxrc_zxr_surface_view_v1));
	if (surface_view == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	surface_view->surface = surface;
	surface_view->view = view;

	uint32_t version = wl_resource_get_version(resource);
	surface_view->resource = wl_resource_create(client,
			&zxr_surface_view_v1_interface, version, surface_view_id);
	if (surface_view->resource == NULL) {
		free(surface_view);
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(surface_view->resource, NULL,
		surface_view, surface_view_handle_resource_destroy);

	wl_list_insert(&surface->surface_views, &surface_view->link);
}

static const struct zxr_surface_v1_interface surface_impl = {
	.get_surface_view = handle_surface_get_surface_view,
};

static void surface_handle_resource_destroy(struct wl_resource *resource) {
	struct wxrc_zxr_surface_v1 *xr_surface = surface_from_resource(resource);
	wl_signal_emit(&xr_surface->events.destroy, xr_surface);
	wl_list_remove(&xr_surface->link);
	free(xr_surface);
}

static void shell_handle_get_xr_surface(struct wl_client *client,
		struct wl_resource *resource, uint32_t id,
		struct wl_resource *surface_resource) {
	struct wxrc_zxr_shell_v1 *shell = shell_from_resource(resource);

	struct wxrc_zxr_surface_v1 *xr_surface =
		calloc(1, sizeof(struct wxrc_zxr_surface_v1));
	if (xr_surface == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	xr_surface->surface = wlr_surface_from_resource(surface_resource);
	wl_list_init(&xr_surface->surface_views);

	wl_signal_init(&xr_surface->events.destroy);

	uint32_t version = wl_resource_get_version(resource);
	xr_surface->resource = wl_resource_create(client,
		&zxr_surface_v1_interface, version, id);
	if (xr_surface->resource == NULL) {
		free(xr_surface);
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(xr_surface->resource, &surface_impl,
		xr_surface, surface_handle_resource_destroy);

	wl_list_insert(&shell->surfaces, &xr_surface->link);

	// TODO: surface destroy listener

	wl_signal_emit(&shell->events.new_surface, xr_surface);
}

static const struct zxr_shell_v1_interface shell_impl = {
	.create_composite_buffer = shell_handle_create_composite_buffer,
	.get_xr_surface = shell_handle_get_xr_surface,
};

static void shell_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wxrc_zxr_shell_v1 *shell = data;

	struct wl_resource *resource = wl_resource_create(client,
		&zxr_shell_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &shell_impl, shell, NULL);
}

static void shell_handle_display_destroy(struct wl_listener *listener,
		void *data) {
	struct wxrc_zxr_shell_v1 *shell =
		wl_container_of(listener, shell, display_destroy);
	wl_signal_emit(&shell->events.destroy, shell);
	wl_list_remove(&shell->display_destroy.link);
	wl_global_destroy(shell->global);
	free(shell);
}

struct wxrc_zxr_shell_v1 *wxrc_zxr_shell_v1_create(
		struct wlr_renderer *renderer, struct wl_display *display) {
	struct wxrc_zxr_shell_v1 *shell =
		calloc(1, sizeof(struct wxrc_zxr_shell_v1));
	if (shell == NULL) {
		return NULL;
	}

	shell->global = wl_global_create(display, &zxr_shell_v1_interface, 1,
		shell, shell_bind);
	if (shell->global == NULL) {
		free(shell);
		return NULL;
	}

	shell->renderer = renderer;
	wl_list_init(&shell->surfaces);
	wl_signal_init(&shell->events.new_surface);
	wl_signal_init(&shell->events.destroy);

	shell->display_destroy.notify = shell_handle_display_destroy;
	wl_display_add_destroy_listener(display, &shell->display_destroy);

	wlr_buffer_register_implementation(&composite_wlr_buffer_impl);

	return shell;
}
