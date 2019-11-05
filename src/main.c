#define _POSIX_C_SOURCE 200112L
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/render/egl.h>
#include <wlr/util/log.h>
#include "backend.h"
#include "render.h"
#include "server.h"
#include "xrutil.h"

static XrResult wxrc_xr_view_push_frame(struct wxrc_xr_view *view,
		struct wxrc_server *server, XrView *xr_view,
		XrCompositionLayerProjectionView *projection_view) {
	XrSwapchainImageAcquireInfo swapchain_image_acquire_info = {
		.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
		.next = NULL,
	};
	uint32_t buffer_index;
	XrResult r = xrAcquireSwapchainImage(view->swapchain,
		&swapchain_image_acquire_info, &buffer_index);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrAcquireSwapchainImage", r);
		return r;
	}

	XrSwapchainImageWaitInfo swapchain_wait_info = {
		.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
		.next = NULL,
		.timeout = 1000,
	};
	r = xrWaitSwapchainImage(view->swapchain, &swapchain_wait_info);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrWaitSwapchainImage", r);
		return r;
	}

	projection_view->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
	projection_view->next = NULL;
	projection_view->pose = xr_view->pose;
	projection_view->fov = xr_view->fov;
	projection_view->subImage.swapchain = view->swapchain;
	projection_view->subImage.imageArrayIndex = buffer_index;
	projection_view->subImage.imageRect.offset.x = 0;
	projection_view->subImage.imageRect.offset.y = 0;
	projection_view->subImage.imageRect.extent.width =
		view->config.recommendedImageRectWidth;
	projection_view->subImage.imageRect.extent.height =
		view->config.recommendedImageRectHeight;

	wxrc_gl_render_view(server, view, xr_view, view->framebuffers[buffer_index],
		view->images[buffer_index].image);
	glFinish();

	XrSwapchainImageReleaseInfo swapchain_release_info = {
		.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
		.next = NULL,
	};
	r = xrReleaseSwapchainImage(view->swapchain, &swapchain_release_info);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrReleaseSwapchainImage", r);
		return r;
	}

	return r;
}

static bool wxrc_xr_push_frame(struct wxrc_server *server,
		XrTime predicted_display_time, XrView *xr_views,
		XrCompositionLayerProjectionView *projection_views) {
	struct wxrc_xr_backend *backend = server->backend;

	for (uint32_t i = 0; i < backend->nviews; i++) {
		xr_views[i].type = XR_TYPE_VIEW;
		xr_views[i].next = NULL;
	}

	XrViewLocateInfo view_locate_info = {
		.type = XR_TYPE_VIEW_LOCATE_INFO,
		.displayTime = predicted_display_time,
		.space = backend->local_space,
	};
	XrViewState view_state = {
		.type = XR_TYPE_VIEW_STATE,
		.next = NULL,
	};
	XrResult r = xrLocateViews(backend->session, &view_locate_info, &view_state,
		backend->nviews, &backend->nviews, xr_views);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrLocateViews", r);
		return false;
	}

	XrFrameBeginInfo frame_begin_info = {
		.type = XR_TYPE_FRAME_BEGIN_INFO,
		.next = NULL,
	};
	r = xrBeginFrame(backend->session, &frame_begin_info);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrBeginFrame", r);
		return false;
	}

	for (uint32_t i = 0; i < backend->nviews; i++) {
		struct wxrc_xr_view *view = &backend->views[i];
		wxrc_xr_view_push_frame(view, server,
			&xr_views[i], &projection_views[i]);
	}

	XrCompositionLayerProjection projection_layer = {
		.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
		.next = NULL,
		.layerFlags = 0,
		.space = backend->local_space,
		.viewCount = backend->nviews,
		.views = projection_views,
	};
	const XrCompositionLayerBaseHeader *projection_layers[] = {
		(XrCompositionLayerBaseHeader *)&projection_layer,
	};
	XrFrameEndInfo frame_end_info = {
		.type = XR_TYPE_FRAME_END_INFO,
		.displayTime = predicted_display_time,
		.layerCount = 1,
		.layers = projection_layers,
		.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
		.next = NULL,
	};
	r = xrEndFrame(backend->session, &frame_end_info);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEndFrame", r);
		return false;
	}

	return true;
}

static void wxrc_xr_handle_event(XrEventDataBuffer *event, bool *running) {
	switch (event->type) {
	case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
		*running = false;
		break;
	case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:;
		XrEventDataSessionStateChanged *state_change_event =
			(XrEventDataSessionStateChanged *)event;
		switch (state_change_event->state) {
		case XR_SESSION_STATE_STOPPING:
		case XR_SESSION_STATE_LOSS_PENDING:
		case XR_SESSION_STATE_EXITING:
			*running = false;
			break;
		default:
			break;
		}
	default:
		break;
	}
}

static int handle_signal(int sig, void *data) {
	bool *running_ptr = data;
	*running_ptr = false;
	return 0;
}

static void handle_new_surface(struct wl_listener *listener, void *data) {
	struct wxrc_server *server = wl_container_of(listener, server, new_surface);
	struct wlr_surface *surface = data;
	server->current_surface = surface;
	/* TODO: add destroy handler */
}

int main(int argc, char *argv[]) {
	struct wxrc_server server = {0};

	wlr_log_init(WLR_DEBUG, NULL);

	server.wl_display = wl_display_create();
	if (server.wl_display == NULL) {
		wlr_log(WLR_ERROR, "wl_display_create failed");
		return 1;
	}
	struct wl_event_loop *wl_event_loop =
		wl_display_get_event_loop(server.wl_display);

	bool running = true;
	struct wl_event_source *signals[] = {
		wl_event_loop_add_signal(wl_event_loop, SIGTERM, handle_signal, &running),
		wl_event_loop_add_signal(wl_event_loop, SIGINT, handle_signal, &running),
	};
	if (signals[0] == NULL || signals[1] == NULL) {
		wlr_log(WLR_ERROR, "wl_event_loop_add_signal failed");
		return 1;
	}

	server.backend = wxrc_xr_backend_create(server.wl_display);
	if (server.backend == NULL) {
		wlr_log(WLR_ERROR, "wxrc_xr_backend_create failed");
		return 1;
	}
	struct wxrc_xr_backend *backend = server.backend;

	if (!wxrc_gl_init(&server.gl)) {
		return 1;
	}

	struct wlr_renderer *renderer = wlr_backend_get_renderer(&backend->base);
	wlr_renderer_init_wl_display(renderer, server.wl_display);

	server.compositor = wlr_compositor_create(server.wl_display, renderer);
	server.new_surface.notify = handle_new_surface;
	wl_signal_add(&server.compositor->events.new_surface, &server.new_surface);

	server.xdg_shell = wlr_xdg_shell_create(server.wl_display);

	const char *wl_socket = wl_display_add_socket_auto(server.wl_display);
	if (wl_socket == NULL) {
		wlr_log(WLR_ERROR, "wl_display_add_socket_auto failed");
		return 1;
	}
	wlr_log(WLR_INFO, "Wayland compositor listening on WAYLAND_DISPLAY=%s",
		wl_socket);

	if (!wlr_backend_start(&backend->base)) {
		wlr_log(WLR_ERROR, "wlr_backend_start failed");
		return 1;
	}

	wlr_log(WLR_DEBUG, "Starting XR main loop");
	XrView *xr_views = calloc(backend->nviews, sizeof(XrView));
	XrCompositionLayerProjectionView *projection_views =
		calloc(backend->nviews, sizeof(XrCompositionLayerProjectionView));
	while (running) {
		XrFrameWaitInfo frame_wait_info = {
			.type = XR_TYPE_FRAME_WAIT_INFO,
			.next = NULL,
		};
		XrFrameState frame_state = {
			.type = XR_TYPE_FRAME_STATE,
			.next = NULL,
		};
		XrResult r = xrWaitFrame(backend->session, &frame_wait_info, &frame_state);
		if (XR_FAILED(r)) {
			wxrc_log_xr_result("xrWaitFrame", r);
			return 1;
		}

		XrEventDataBuffer event = {
			.type = XR_TYPE_EVENT_DATA_BUFFER,
			.next = NULL,
		};
		r = xrPollEvent(backend->instance, &event);
		if (r != XR_EVENT_UNAVAILABLE) {
			if (XR_FAILED(r)) {
				wxrc_log_xr_result("xrPollEvent", r);
				return 1;
			}
			wxrc_xr_handle_event(&event, &running);
		}

		wl_display_flush_clients(server.wl_display);
		int ret = wl_event_loop_dispatch(wl_event_loop, 1);
		if (ret < 0) {
			wlr_log(WLR_ERROR, "wl_event_loop_dispatch failed");
			return 1;
		}

		if (!running) {
			break;
		}

		if (!wxrc_xr_push_frame(&server, frame_state.predictedDisplayTime,
				xr_views, projection_views)) {
			return 1;
		}

		if (server.current_surface != NULL) {
			struct timespec now = {0};
			wlr_surface_send_frame_done(server.current_surface, &now);
		}
	}

	wlr_log(WLR_DEBUG, "Tearing down XR instance");
	free(projection_views);
	free(xr_views);
	wl_event_source_remove(signals[0]);
	wl_event_source_remove(signals[1]);
	wxrc_gl_finish(&server.gl);
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
