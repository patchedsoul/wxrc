#include <assert.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
#include "backend.h"

struct wxrc_xr_backend *get_xr_backend_from_backend(
		struct wlr_backend *wlr_backend) {
	assert(wxrc_backend_is_xr(wlr_backend));
	return (struct wxrc_xr_backend *)wlr_backend;
}

static bool backend_start(struct wlr_backend *backend) {
	wlr_log(WLR_DEBUG, "Starting wlroots XR backend");
	return true;
}

static void backend_destroy(struct wlr_backend *wlr_backend) {
	if (!wlr_backend) {
		return;
	}

	struct wxrc_xr_backend *backend = get_xr_backend_from_backend(wlr_backend);

	wl_signal_emit(&backend->base.events.destroy, &backend->base);

	wl_list_remove(&backend->local_display_destroy.link);

	wlr_renderer_destroy(backend->renderer);
	wlr_egl_finish(&backend->egl);

	wl_display_disconnect(backend->remote_display);

	free(backend);
}

static struct wlr_renderer *backend_get_renderer(
		struct wlr_backend *wlr_backend) {
	struct wxrc_xr_backend *backend = get_xr_backend_from_backend(wlr_backend);
	return backend->renderer;
}

static struct wlr_backend_impl backend_impl = {
	.start = backend_start,
	.destroy = backend_destroy,
	.get_renderer = backend_get_renderer,
};

bool wxrc_backend_is_xr(struct wlr_backend *wlr_backend) {
	return wlr_backend->impl == &backend_impl;
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wxrc_xr_backend *backend =
		wl_container_of(listener, backend, local_display_destroy);
	backend_destroy(&backend->base);
}

struct wxrc_xr_backend *wxrc_xr_backend_create(struct wl_display *display) {
	struct wxrc_xr_backend *backend = calloc(1, sizeof(*backend));
	if (backend == NULL) {
		wlr_log_errno(WLR_ERROR, "calloc failed");
		return NULL;
	}
	wlr_backend_init(&backend->base, &backend_impl);

	/* TODO: support more platforms */
	backend->remote_display = wl_display_connect(NULL);
	if (backend->remote_display == NULL) {
		wlr_log(WLR_ERROR, "wl_display_connect failed");
		return NULL;
	}

	EGLint egl_config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE,
	};
	backend->renderer = wlr_renderer_autocreate(&backend->egl,
		EGL_PLATFORM_WAYLAND_EXT, backend->remote_display, egl_config_attribs,
		WL_SHM_FORMAT_ARGB8888);
	if (backend->renderer == NULL) {
		wlr_log(WLR_ERROR, "wlr_renderer_autocreate failed");
		wl_display_disconnect(backend->remote_display);
		return NULL;
	}

	backend->local_display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &backend->local_display_destroy);

	return backend;
}
