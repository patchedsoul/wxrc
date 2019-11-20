#ifndef _WXRC_BACKEND_H
#define _WXRC_BACKEND_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <wlr/backend/interface.h>
#include <wlr/render/wlr_renderer.h>
#include "xr-shell-protocol.h"

struct wxrc_xr_view {
	XrViewConfigurationView config;
	XrSwapchain swapchain;

	uint32_t nimages;
	XrSwapchainImageOpenGLESKHR *images;
	GLuint *framebuffers;
	GLuint depth_buffer;

	struct wxrc_zxr_view_v1 *wl_view;
};

struct wxrc_xr_backend {
	struct wlr_backend base;

	bool started;

	struct wlr_egl *egl;
	struct wlr_renderer *renderer;

	XrInstance instance;
	XrSystemId sysid;
	XrSession session;

	XrSpace local_space;

	uint32_t nviews;
	struct wxrc_xr_view *views;

	struct wl_listener local_display_destroy;
};

bool wxrc_backend_is_xr(struct wlr_backend *wlr_backend);
struct wxrc_xr_backend *wxrc_xr_backend_create(
		struct wl_display *display, struct wlr_renderer *renderer);

#endif
