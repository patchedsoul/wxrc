#define _POSIX_C_SOURCE 200112L
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/multi.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/render/egl.h>
#include <wlr/util/log.h>
#include "backend.h"
#include "render.h"
#include "server.h"
#include "view.h"
#include "xrutil.h"

static XrResult wxrc_xr_view_push_frame(struct wxrc_xr_view *view,
		struct wxrc_server *server, XrView *xr_view,
		XrCompositionLayerProjectionView *projection_view) {
	uint32_t buffer_index;
	XrResult r = xrAcquireSwapchainImage(view->swapchain, NULL, &buffer_index);
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

	r = xrReleaseSwapchainImage(view->swapchain, NULL);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrReleaseSwapchainImage", r);
		return r;
	}

	return r;
}

static bool wxrc_xr_push_frame(struct wxrc_server *server,
		XrTime predicted_display_time, XrView *xr_views,
		XrCompositionLayerProjectionView *projection_views) {
	struct wxrc_xr_backend *backend = server->xr_backend;

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

	r = xrBeginFrame(backend->session, NULL);
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

static void send_geometry(struct wl_resource *resource) {
	wl_output_send_geometry(resource, 0, 0,
		1200, 1200, WL_OUTPUT_SUBPIXEL_UNKNOWN,
		"wxrc", "wxrc", WL_OUTPUT_TRANSFORM_NORMAL);
}

static void send_all_modes(struct wl_resource *resource) {
	wl_output_send_mode(resource, WL_OUTPUT_MODE_CURRENT,
		1920, 1080, 144000);
}

static void send_scale(struct wl_resource *resource) {
	uint32_t version = wl_resource_get_version(resource);
	if (version >= WL_OUTPUT_SCALE_SINCE_VERSION) {
		wl_output_send_scale(resource, 1);
	}
}

static void send_done(struct wl_resource *resource) {
	uint32_t version = wl_resource_get_version(resource);
	if (version >= WL_OUTPUT_DONE_SINCE_VERSION) {
		wl_output_send_done(resource);
	}
}

static void output_handle_resource_destroy(struct wl_resource *resource) {
	// This space deliberately left blank
}

static void output_handle_release(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wl_output_interface output_impl = {
	.release = output_handle_release,
};

static void output_bind(struct wl_client *wl_client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_output *output = data;

	struct wl_resource *resource = wl_resource_create(wl_client,
		&wl_output_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	wl_resource_set_implementation(resource, &output_impl, output,
		output_handle_resource_destroy);

	send_geometry(resource);
	send_all_modes(resource);
	send_scale(resource);
	send_done(resource);
}

static struct wlr_egl *native_egl;

struct wlr_renderer *create_renderer(struct wlr_egl *egl, EGLenum platform,
		void *remote_display, EGLint *config_attribs, EGLint visual_id) {
	/* HACK HACK HACK */
	native_egl = egl;
	return wlr_renderer_autocreate(egl, platform,
			remote_display, config_attribs, visual_id);
}

int main(int argc, char *argv[]) {
	struct wxrc_server server = {0};

	wlr_log_init(WLR_DEBUG, NULL);

	const char *startup_cmd = NULL;
	int opt;
	while ((opt = getopt(argc, argv, "s:h")) != -1) {
		switch (opt) {
		case 's':
			startup_cmd = optarg;
			break;
		default:
			fprintf(stderr, "usage: %s [-s startup-cmd]\n", argv[0]);
			return 1;
		}
	}

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

	server.backend = wlr_backend_autocreate(server.wl_display, create_renderer);
	if (server.backend == NULL) {
		wlr_log(WLR_ERROR, "Failed to create native backend");
		return 1;
	}

	struct wlr_renderer *renderer = wlr_backend_get_renderer(server.backend);
	server.xr_backend = wxrc_xr_backend_create(
			server.wl_display, renderer, native_egl);
	if (server.xr_backend == NULL || server.backend == NULL) {
		wlr_log(WLR_ERROR, "backend creation failed");
		return 1;
	}
	struct wxrc_xr_backend *xr_backend = server.xr_backend;
	wlr_multi_backend_add(server.backend, &xr_backend->base);

	if (!wxrc_gl_init(&server.gl)) {
		return 1;
	}

	wlr_renderer_init_wl_display(renderer, server.wl_display);

	wlr_compositor_create(server.wl_display, renderer);
	wlr_data_device_manager_create(server.wl_display);

	server.seat = wlr_seat_create(server.wl_display, "seat0");

	wl_list_init(&server.views);
	wxrc_xdg_shell_init(&server);

	const char *wl_socket = wl_display_add_socket_auto(server.wl_display);
	if (wl_socket == NULL) {
		wlr_log(WLR_ERROR, "wl_display_add_socket_auto failed");
		return 1;
	}
	wlr_log(WLR_INFO, "Wayland compositor listening on WAYLAND_DISPLAY=%s",
		wl_socket);

	if (!wlr_backend_start(server.backend)) {
		wlr_log(WLR_ERROR, "wlr_backend_start failed");
		return 1;
	}

	setenv("WAYLAND_DISPLAY", wl_socket, true);
	if (startup_cmd != NULL) {
		pid_t pid = fork();
		if (pid < 0) {
			wlr_log_errno(WLR_ERROR, "fork failed");
			return 1;
		} else if (pid == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
			wlr_log_errno(WLR_ERROR, "execl failed");
			exit(1);
		}
	}

	wl_global_create(server.wl_display, &wl_output_interface,
			3, NULL, output_bind);

	wlr_log(WLR_DEBUG, "Starting XR main loop");
	server.xr_views = calloc(xr_backend->nviews, sizeof(XrView));
	XrCompositionLayerProjectionView *projection_views =
		calloc(xr_backend->nviews, sizeof(XrCompositionLayerProjectionView));
	while (running) {
		XrFrameState frame_state = {
			.type = XR_TYPE_FRAME_STATE,
			.next = NULL,
		};
		XrResult r = xrWaitFrame(xr_backend->session, NULL, &frame_state);
		if (XR_FAILED(r)) {
			wxrc_log_xr_result("xrWaitFrame", r);
			return 1;
		}

		XrEventDataBuffer event = {
			.type = XR_TYPE_EVENT_DATA_BUFFER,
			.next = NULL,
		};
		r = xrPollEvent(xr_backend->instance, &event);
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
				server.xr_views, projection_views)) {
			return 1;
		}

		struct timespec now;
		/* TODO: Derive this from predictedDisplayTime */
		clock_gettime(CLOCK_MONOTONIC, &now);

		struct wxrc_view *view;
		wl_list_for_each(view, &server.views, link) {
			wlr_surface_send_frame_done(view->surface, &now);
		}
	}

	wlr_log(WLR_DEBUG, "Tearing down XR instance");
	free(projection_views);
	free(server.xr_views);
	wl_event_source_remove(signals[0]);
	wl_event_source_remove(signals[1]);
	wxrc_gl_finish(&server.gl);
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
