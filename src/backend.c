#include <assert.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
#include "backend.h"
#include "xrutil.h"

struct wxrc_xr_backend *get_xr_backend_from_backend(
		struct wlr_backend *wlr_backend) {
	assert(wxrc_backend_is_xr(wlr_backend));
	return (struct wxrc_xr_backend *)wlr_backend;
}

static XrResult wxrc_xr_enumerate_layer_props(void) {
	uint32_t nprops;
	XrApiLayerProperties *props = NULL;
	XrResult r = xrEnumerateApiLayerProperties(0, &nprops, NULL);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateApiLayerProperties", r);
		goto exit;
	}
	wlr_log(WLR_DEBUG, "OpenXR API layer properties (%d):", nprops);
	props = calloc(sizeof(XrApiLayerProperties), nprops);
	for (uint32_t i = 0; i < nprops; ++i) {
		XrApiLayerProperties *prop = &props[i];
		prop->type = XR_TYPE_API_LAYER_PROPERTIES;
	}

	r = xrEnumerateApiLayerProperties(nprops, &nprops, props);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateApiLayerProperties", r);
		goto exit;
	}

	for (uint32_t i = 0; i < nprops; ++i) {
		XrApiLayerProperties *prop = &props[i];
		wlr_log(WLR_DEBUG, "%s: spec version %d; layer version %d; %s",
				prop->layerName, (int)prop->specVersion, prop->layerVersion,
				prop->description);
	}

exit:
	free(props);
	return r;
}

static XrResult wxrc_xr_enumerate_instance_props(void) {
	uint32_t nprops;
	XrExtensionProperties *props = NULL;
	XrResult r = xrEnumerateInstanceExtensionProperties(NULL, 0, &nprops, NULL);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateInstanceExtensionProperties", r);
		goto exit;
	}
	wlr_log(WLR_DEBUG, "OpenXR Instance extension properties (%d):", nprops);
	props = calloc(sizeof(XrExtensionProperties), nprops);
	for (uint32_t i = 0; i < nprops; ++i) {
		XrExtensionProperties *prop = &props[i];
		prop->type = XR_TYPE_EXTENSION_PROPERTIES;
	}

	r = xrEnumerateInstanceExtensionProperties(NULL, nprops, &nprops, props);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateInstanceExtensionProperties", r);
		goto exit;
	}

	for (uint32_t i = 0; i < nprops; ++i) {
		XrExtensionProperties *prop = &props[i];
		wlr_log(WLR_DEBUG, "\t%s v%d", prop->extensionName,
				prop->extensionVersion);
	}

exit:
	free(props);
	return r;
}

static XrResult wxrc_create_xr_instance(XrInstance *instance) {
	const char *extensions[] = {
		XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
		XR_MND_EGL_ENABLE_EXTENSION_NAME,
	};
	XrInstanceCreateInfo info = {
		.type = XR_TYPE_INSTANCE_CREATE_INFO,
		.next = NULL,
		.createFlags = 0,
		.applicationInfo = {
			.applicationName = "wxrc",
			.applicationVersion = 0,
			.engineName = "",
			.engineVersion = 0,
			.apiVersion = XR_CURRENT_API_VERSION,
		},
		.enabledApiLayerCount = 0,
		.enabledApiLayerNames = NULL,
		.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]),
		.enabledExtensionNames = extensions,
	};
	wlr_log(WLR_DEBUG, "Creating XR instance");
	XrResult r = xrCreateInstance(&info, instance);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrCreateInstance", r);
		return r;
	}

	XrInstanceProperties iprops = {XR_TYPE_INSTANCE_PROPERTIES};
	r = xrGetInstanceProperties(*instance, &iprops);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrGetInstanceProperties", r);
		return r;
	}
	wlr_log(WLR_DEBUG, "XR runtime %s v%d.%d.%d",
			iprops.runtimeName,
			XR_VERSION_MAJOR(iprops.runtimeVersion),
			XR_VERSION_MINOR(iprops.runtimeVersion),
			XR_VERSION_PATCH(iprops.runtimeVersion));
	return r;
}

static XrResult wxrc_get_xr_system(XrInstance instance, XrSystemId *sysid) {
	/* XXX: Do we care about handheld devices? */
	XrSystemGetInfo sysinfo = {
		.type = XR_TYPE_SYSTEM_GET_INFO,
		.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
	};
	XrResult r = xrGetSystem(instance, &sysinfo, sysid);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrGetSystem", r);
		return r;
	}

	XrSystemProperties props = {XR_TYPE_SYSTEM_PROPERTIES};
	xrGetSystemProperties(instance, *sysid, &props);
	if (XR_FAILED(r)) {
		return r;
	}

	wlr_log(WLR_DEBUG, "XR system %s; vendor ID: %d",
			props.systemName, props.vendorId);
	wlr_log(WLR_DEBUG, "\tmax swapchain size %dx%d:%d",
			props.graphicsProperties.maxSwapchainImageWidth,
			props.graphicsProperties.maxSwapchainImageHeight,
			props.graphicsProperties.maxLayerCount);
	wlr_log(WLR_DEBUG, "\torientation: %s; position: %s",
			props.trackingProperties.orientationTracking ? "yes" : "no",
			props.trackingProperties.positionTracking ? "yes" : "no");

	return r;
}

static XrResult wxrc_xr_enumerate_view_configs(XrInstance instance,
		XrSystemId sysid) {
	uint32_t nconfigs;
	XrResult r =
		xrEnumerateViewConfigurations(instance, sysid, 0, &nconfigs, NULL);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateViewConfigurations", r);
		return r;
	}

	XrViewConfigurationType *configs =
		calloc(nconfigs, sizeof(XrViewConfigurationType));
	if (configs == NULL) {
		wlr_log_errno(WLR_ERROR, "calloc failed");
		return XR_ERROR_OUT_OF_MEMORY;
	}
	r = xrEnumerateViewConfigurations(instance, sysid, nconfigs,
		&nconfigs, configs);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateViewConfigurations", r);
		goto exit;
	}

	bool found = false;
	for (size_t i = 0; i < nconfigs; i++) {
		if (configs[i] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
			found = true;
			break;
		}
	}
	if (!found) {
		wlr_log(WLR_ERROR, "OpenXR system doesn't support stereo view config");
		return XR_ERROR_INITIALIZATION_FAILED;
	}

	XrViewConfigurationProperties stereo_props = {
		.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
		.next = NULL,
	};
	r = xrGetViewConfigurationProperties(instance, sysid,
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &stereo_props);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrGetViewConfigurationProperties", r);
		goto exit;
	}

	wlr_log(WLR_DEBUG, "OpenXR stereo view config: FOV %s",
		stereo_props.fovMutable ? "mutable" : "immutable");

exit:
	free(configs);
	return r;
}

static XrViewConfigurationView *wxrc_xr_enumerate_stereo_config_views(
		XrInstance instance, XrSystemId sysid, uint32_t *nviews) {
	XrResult r = xrEnumerateViewConfigurationViews(instance, sysid,
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, nviews, NULL);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateViewConfigurationViews", r);
		return NULL;
	}

	XrViewConfigurationView *views =
		calloc(*nviews, sizeof(XrViewConfigurationView));
	if (views == NULL) {
		wlr_log_errno(WLR_ERROR, "calloc failed");
		return NULL;
	}
	r = xrEnumerateViewConfigurationViews(instance, sysid,
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, *nviews, nviews, views);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateViewConfigurationViews", r);
		free(views);
		return NULL;
	}

	wlr_log(WLR_DEBUG, "Stereo views (%d):", *nviews);
	for (size_t i = 0; i < *nviews; i++) {
		wlr_log(WLR_DEBUG, "\t%zu: recommended resolution %xx%d, "
			"max resolution %dx%d, recommended swapchain samples %d, "
			"max swapchain samples %d",
			i,
			views[i].recommendedImageRectWidth,
			views[i].recommendedImageRectHeight,
			views[i].maxImageRectWidth, views[i].maxImageRectHeight,
			views[i].recommendedSwapchainSampleCount,
			views[i].maxSwapchainSampleCount);
	}

	return views;
}

static XrResult wxrc_create_xr_session(XrInstance instance,
		XrSystemId sysid, XrSession *session, struct wlr_egl *egl) {
	PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR;
	XrResult r = xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR",
		(PFN_xrVoidFunction *)&xrGetOpenGLESGraphicsRequirementsKHR);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrGetInstanceProcAddr "
			"(xrGetOpenGLESGraphicsRequirementsKHR)", r);
		return r;
	}

	XrGraphicsRequirementsOpenGLESKHR reqs =
		{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
	r = xrGetOpenGLESGraphicsRequirementsKHR(instance, sysid, &reqs);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrGetOpenGLESGraphicsRequirementsKHR", r);
		return r;
	}

	wlr_log(WLR_DEBUG, "OpenGL ES gfx requirements: min %d.%d.%d, max: %d.%d.%d",
			XR_VERSION_MAJOR(reqs.minApiVersionSupported),
			XR_VERSION_MINOR(reqs.minApiVersionSupported),
			XR_VERSION_PATCH(reqs.minApiVersionSupported),
			XR_VERSION_MAJOR(reqs.maxApiVersionSupported),
			XR_VERSION_MINOR(reqs.maxApiVersionSupported),
			XR_VERSION_PATCH(reqs.maxApiVersionSupported));

	EGLint gles_version = 2;
	if (XR_VERSION_MAJOR(reqs.minApiVersionSupported) > gles_version ||
			XR_VERSION_MAJOR(reqs.maxApiVersionSupported) < gles_version) {
		wlr_log(WLR_ERROR, "XR runtime does not support a suitable GL version");
		return XR_ERROR_INITIALIZATION_FAILED;
	}

	XrGraphicsBindingEGLMND gfx = {
		.type = XR_TYPE_GRAPHICS_BINDING_EGL_MND,
		.getProcAddress = eglGetProcAddress,
		.display = egl->display,
		.config = egl->config,
		.context = egl->context,
	};

	XrSessionCreateInfo sessinfo = {
		.type = XR_TYPE_SESSION_CREATE_INFO,
		.next = &gfx,
		.createFlags = 0,
		.systemId = sysid,
	};

	r = xrCreateSession(instance, &sessinfo, session);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrCreateSession", r);
	}
	return r;
}

static XrResult wxrc_xr_enumerate_reference_spaces(XrSession session) {
	uint32_t nspaces;
	XrResult r = xrEnumerateReferenceSpaces(session, 0, &nspaces, NULL);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateReferenceSpaces", r);
		return r;
	}

	XrReferenceSpaceType *spaces = calloc(nspaces, sizeof(XrReferenceSpaceType));
	if (spaces == NULL) {
		wlr_log_errno(WLR_ERROR, "calloc failed");
		return XR_ERROR_OUT_OF_MEMORY;
	}

	r = xrEnumerateReferenceSpaces(session, nspaces, &nspaces, spaces);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateReferenceSpaces", r);
		goto exit;
	}

	wlr_log(WLR_DEBUG, "OpenXR reference spaces (%d):", nspaces);
	bool has_local = false;
	for (uint32_t i = 0; i < nspaces; i++) {
		const char *name;
		switch (spaces[i]) {
		case XR_REFERENCE_SPACE_TYPE_LOCAL:
			name = "local";
			has_local = true;
			break;
		case XR_REFERENCE_SPACE_TYPE_STAGE:
			name = "stage";
			break;
		case XR_REFERENCE_SPACE_TYPE_VIEW:
			name = "view";
			break;
		default:
			name = NULL;
		}
		if (name == NULL) {
			continue;
		}
		wlr_log(WLR_DEBUG, "\t%s", name);
	}

	if (!has_local) {
		wlr_log(WLR_ERROR, "Local reference space not supported");
		r = XR_ERROR_INITIALIZATION_FAILED;
	}
exit:
	free(spaces);
	return r;
}

static XrResult wxrc_xr_create_local_reference_space(XrSession session,
		XrSpace *space) {
	XrPosef identity_pose = {
		.orientation = { .x = 0, .y = 0, .z = 0, .w = 1 },
		.position = { .x = 0, .y = 0, .z = 0 },
	};
	XrReferenceSpaceCreateInfo create_info = {
		.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
		.next = NULL,
		.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
		.poseInReferenceSpace = identity_pose,
	};
	XrResult r = xrCreateReferenceSpace(session, &create_info, space);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrCreateReferenceSpace", r);
	}
	return r;
}

static struct wxrc_xr_view *wxrc_xr_create_swapchains(XrSession session,
		uint32_t nviews, XrViewConfigurationView *view_configs) {
	uint32_t nformats;
	XrResult r = xrEnumerateSwapchainFormats(session, 0, &nformats, NULL);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateSwapchainFormats", r);
		return NULL;
	}

	int64_t *formats = calloc(nformats, sizeof(int64_t));
	if (formats == NULL) {
		wlr_log_errno(WLR_ERROR, "calloc failed");
		return NULL;
	}
	r = xrEnumerateSwapchainFormats(session, nformats, &nformats, formats);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrEnumerateSwapchainFormats", r);
		free(formats);
		return NULL;
	}

	int64_t format = formats[0];
	wlr_log(WLR_DEBUG, "XR runtime supports %d swapchain formats, "
		"picking format %" PRIi64, nformats, format);

	free(formats);

	struct wxrc_xr_view *views = calloc(nviews, sizeof(struct wxrc_xr_view));
	if (views == NULL) {
		wlr_log_errno(WLR_ERROR, "calloc failed");
		return NULL;
	}

	for (uint32_t i = 0; i < nviews; i++) {
		views[i].config = view_configs[i];

		XrSwapchainCreateInfo create_info = {
			.type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
			.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
				XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
			.createFlags = 0,
			.format = format,
			.sampleCount = 1,
			.width = views[i].config.recommendedImageRectWidth,
			.height = views[i].config.recommendedImageRectHeight,
			.faceCount = 1,
			.arraySize = 1,
			.mipCount = 1,
			.next = NULL,
		};
		r = xrCreateSwapchain(session, &create_info, &views[i].swapchain);
		if (XR_FAILED(r)) {
			wxrc_log_xr_result("xrCreateSwapchain", r);
			goto error;
		}
	}

	for (uint32_t i = 0; i < nviews; i++) {
		struct wxrc_xr_view *view = &views[i];
		r = xrEnumerateSwapchainImages(view->swapchain, 0,
			&view->nimages, NULL);
		if (XR_FAILED(r)) {
			wxrc_log_xr_result("xrEnumerateSwapchainImages", r);
			goto error;
		}

		view->images =
			calloc(view->nimages, sizeof(XrSwapchainImageOpenGLESKHR));
		if (view->images == NULL) {
			wlr_log_errno(WLR_ERROR, "calloc failed");
			r = XR_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		for (uint32_t j = 0; j < view->nimages; j++) {
			view->images[j].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
			view->images[j].next = NULL;
		}
		r = xrEnumerateSwapchainImages(view->swapchain, view->nimages,
			&view->nimages, (XrSwapchainImageBaseHeader *)view->images);
		if (XR_FAILED(r)) {
			wxrc_log_xr_result("xrEnumerateSwapchainImages", r);
			free(view->images);
			goto error;
		}

		view->framebuffers = calloc(view->nimages, sizeof(GLuint));
		if (view->framebuffers == NULL) {
			wlr_log_errno(WLR_ERROR, "calloc failed");
			r = XR_ERROR_OUT_OF_MEMORY;
			goto error;
		}

		glGenFramebuffers(view->nimages, view->framebuffers);
	}

	return views;

error:
	free(views);
	return NULL;
}

static bool wxrc_xr_init(struct wxrc_xr_backend *backend) {
	if (XR_FAILED(wxrc_xr_enumerate_layer_props())) {
		return false;
	}
	if (XR_FAILED(wxrc_xr_enumerate_instance_props())) {
		return false;
	}

	XrResult r = wxrc_create_xr_instance(&backend->instance);
	if (XR_FAILED(r)) {
		return false;
	}

	r = wxrc_get_xr_system(backend->instance, &backend->sysid);
	if (XR_FAILED(r)) {
		return false;
	}

	if (XR_FAILED(wxrc_xr_enumerate_view_configs(backend->instance,
			backend->sysid))) {
		return false;
	}

	return true;
}

static void wxrc_xr_view_finish(struct wxrc_xr_view *view) {
	glDeleteFramebuffers(view->nimages, view->framebuffers);
	free(view->framebuffers);
	free(view->images);
	xrDestroySwapchain(view->swapchain);
}

static bool backend_start(struct wlr_backend *wlr_backend) {
	struct wxrc_xr_backend *backend = get_xr_backend_from_backend(wlr_backend);
	assert(!backend->started);

	wlr_log(WLR_DEBUG, "Starting wlroots XR backend");

	XrViewConfigurationView *view_configs = wxrc_xr_enumerate_stereo_config_views(
		backend->instance, backend->sysid, &backend->nviews);
	if (view_configs == NULL) {
		return false;
	}

	XrResult r = wxrc_create_xr_session(backend->instance, backend->sysid,
		&backend->session, &backend->egl);
	if (XR_FAILED(r)) {
		return false;
	}

	if (XR_FAILED(wxrc_xr_enumerate_reference_spaces(backend->session))) {
		return 1;
	}

	r = wxrc_xr_create_local_reference_space(backend->session, &backend->local_space);
	if (XR_FAILED(r)) {
		return false;
	}

	wlr_log(WLR_DEBUG, "Starting XR session");
	XrSessionBeginInfo session_begin_info = {
		.type = XR_TYPE_SESSION_BEGIN_INFO,
		.next = NULL,
		.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
	};
	r = xrBeginSession(backend->session, &session_begin_info);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrBeginSession", r);
		return false;
	}

	backend->views =
		wxrc_xr_create_swapchains(backend->session, backend->nviews, view_configs);
	if (backend->views == NULL) {
		return false;
	}

	free(view_configs);
	backend->started = true;

	return true;
}

static void backend_destroy(struct wlr_backend *wlr_backend) {
	if (!wlr_backend) {
		return;
	}

	struct wxrc_xr_backend *backend = get_xr_backend_from_backend(wlr_backend);

	wl_signal_emit(&backend->base.events.destroy, &backend->base);

	wl_list_remove(&backend->local_display_destroy.link);

	if (backend->started) {
		for (uint32_t i = 0; i < backend->nviews; i++) {
			wxrc_xr_view_finish(&backend->views[i]);
		}
		free(backend->views);
		xrDestroySpace(backend->local_space);
	}
	xrDestroySession(backend->session);
	xrDestroyInstance(backend->instance);

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

	if (!wxrc_xr_init(backend)) {
		return NULL;
	}

	backend->local_display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &backend->local_display_destroy);

	return backend;
}
