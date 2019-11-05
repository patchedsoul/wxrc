#define _POSIX_C_SOURCE 200112L
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-server.h>
#include <wlr/render/egl.h>
#include <wlr/util/log.h>
#include "backend.h"
#include "render.h"
#include "xr.h"
#include "xrutil.h"

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

XrResult wxrc_create_xr_instance(XrInstance *instance) {
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

XrResult wxrc_get_xr_system(XrInstance instance, XrSystemId *sysid) {
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

XrViewConfigurationView *wxrc_xr_enumerate_stereo_config_views(
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

XrResult wxrc_create_xr_session(XrInstance instance,
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

XrResult wxrc_xr_enumerate_reference_spaces(XrSession session) {
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

XrResult wxrc_xr_create_local_reference_space(XrSession session,
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

struct wxrc_xr_view *wxrc_xr_create_swapchains(XrSession session,
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

static bool wxrc_xr_init(struct wxrc_xr *xr, struct wxrc_xr_backend *backend) {
	if (XR_FAILED(wxrc_xr_enumerate_layer_props())) {
		return false;
	}
	if (XR_FAILED(wxrc_xr_enumerate_instance_props())) {
		return false;
	}

	XrResult r = wxrc_create_xr_instance(&xr->instance);
	if (XR_FAILED(r)) {
		return false;
	}

	XrSystemId sysid;
	r = wxrc_get_xr_system(xr->instance, &sysid);
	if (XR_FAILED(r)) {
		return false;
	}

	if (XR_FAILED(wxrc_xr_enumerate_view_configs(xr->instance, sysid))) {
		return false;
	}

	XrViewConfigurationView *view_configs =
		wxrc_xr_enumerate_stereo_config_views(xr->instance, sysid, &xr->nviews);
	if (view_configs == NULL) {
		return false;
	}

	r = wxrc_create_xr_session(xr->instance, sysid, &xr->session, &backend->egl);
	if (XR_FAILED(r)) {
		return false;
	}

	if (XR_FAILED(wxrc_xr_enumerate_reference_spaces(xr->session))) {
		return 1;
	}

	r = wxrc_xr_create_local_reference_space(xr->session, &xr->local_space);
	if (XR_FAILED(r)) {
		return false;
	}

	wlr_log(WLR_DEBUG, "Starting XR session");
	XrSessionBeginInfo session_begin_info = {
		.type = XR_TYPE_SESSION_BEGIN_INFO,
		.next = NULL,
		.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
	};
	r = xrBeginSession(xr->session, &session_begin_info);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrBeginSession", r);
		return false;
	}

	xr->views =
		wxrc_xr_create_swapchains(xr->session, xr->nviews, view_configs);
	if (xr->views == NULL) {
		return false;
	}

	free(view_configs);

	return true;
}

static void wxrc_xr_view_finish(struct wxrc_xr_view *view) {
	glDeleteFramebuffers(view->nimages, view->framebuffers);
	free(view->framebuffers);
	free(view->images);
	xrDestroySwapchain(view->swapchain);
}

static void wxrc_xr_finish(struct wxrc_xr *xr) {
	for (uint32_t i = 0; i < xr->nviews; i++) {
		wxrc_xr_view_finish(&xr->views[i]);
	}
	free(xr->views);
	xrDestroySpace(xr->local_space);
	xrDestroySession(xr->session);
	xrDestroyInstance(xr->instance);
}

static XrResult wxrc_xr_view_push_frame(struct wxrc_xr_view *view,
		struct wxrc_gl *gl, XrView *xr_view,
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

	wxrc_gl_render_view(gl, view, xr_view, view->framebuffers[buffer_index],
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

static bool wxrc_xr_push_frame(struct wxrc_xr *xr, struct wxrc_gl *gl,
		XrTime predicted_display_time, XrView *xr_views,
		XrCompositionLayerProjectionView *projection_views) {
	for (uint32_t i = 0; i < xr->nviews; i++) {
		xr_views[i].type = XR_TYPE_VIEW;
		xr_views[i].next = NULL;
	}

	XrViewLocateInfo view_locate_info = {
		.type = XR_TYPE_VIEW_LOCATE_INFO,
		.displayTime = predicted_display_time,
		.space = xr->local_space,
	};
	XrViewState view_state = {
		.type = XR_TYPE_VIEW_STATE,
		.next = NULL,
	};
	XrResult r = xrLocateViews(xr->session, &view_locate_info, &view_state,
		xr->nviews, &xr->nviews, xr_views);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrLocateViews", r);
		return false;
	}

	XrFrameBeginInfo frame_begin_info = {
		.type = XR_TYPE_FRAME_BEGIN_INFO,
		.next = NULL,
	};
	r = xrBeginFrame(xr->session, &frame_begin_info);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrBeginFrame", r);
		return false;
	}

	for (uint32_t i = 0; i < xr->nviews; i++) {
		struct wxrc_xr_view *view = &xr->views[i];
		wxrc_xr_view_push_frame(view, gl, &xr_views[i], &projection_views[i]);
	}

	XrCompositionLayerProjection projection_layer = {
		.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
		.next = NULL,
		.layerFlags = 0,
		.space = xr->local_space,
		.viewCount = xr->nviews,
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
	r = xrEndFrame(xr->session, &frame_end_info);
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

int main(int argc, char *argv[]) {
	wlr_log_init(WLR_DEBUG, NULL);

	struct wl_display *wl_display = wl_display_create();
	if (wl_display == NULL) {
		wlr_log(WLR_ERROR, "wl_display_create failed");
		return 1;
	}
	struct wl_event_loop *wl_event_loop = wl_display_get_event_loop(wl_display);

	bool running = true;
	struct wl_event_source *signals[] = {
		wl_event_loop_add_signal(wl_event_loop, SIGTERM, handle_signal, &running),
		wl_event_loop_add_signal(wl_event_loop, SIGINT, handle_signal, &running),
	};
	if (signals[0] == NULL || signals[1] == NULL) {
		wlr_log(WLR_ERROR, "wl_event_loop_add_signal failed");
		return 1;
	}

	struct wxrc_xr_backend *backend = wxrc_xr_backend_create(wl_display);
	if (backend == NULL) {
		wlr_log(WLR_ERROR, "wxrc_xr_backend_create failed");
		return 1;
	}

	struct wxrc_xr xr = {0};
	if (!wxrc_xr_init(&xr, backend)) {
		return 1;
	}

	struct wxrc_gl gl = {0};
	if (!wxrc_gl_init(&gl)) {
		return 1;
	}

	const char *wl_socket = wl_display_add_socket_auto(wl_display);
	if (wl_socket == NULL) {
		wlr_log(WLR_ERROR, "wl_display_add_socket_auto failed");
		return 1;
	}
	wlr_log(WLR_INFO, "Wayland compositor listening on %s", wl_socket);

	wlr_log(WLR_DEBUG, "Starting XR main loop");
	XrView *xr_views = calloc(xr.nviews, sizeof(XrView));
	XrCompositionLayerProjectionView *projection_views =
		calloc(xr.nviews, sizeof(XrCompositionLayerProjectionView));
	while (running) {
		XrFrameWaitInfo frame_wait_info = {
			.type = XR_TYPE_FRAME_WAIT_INFO,
			.next = NULL,
		};
		XrFrameState frame_state = {
			.type = XR_TYPE_FRAME_STATE,
			.next = NULL,
		};
		XrResult r = xrWaitFrame(xr.session, &frame_wait_info, &frame_state);
		if (XR_FAILED(r)) {
			wxrc_log_xr_result("xrWaitFrame", r);
			return 1;
		}

		XrEventDataBuffer event = {
			.type = XR_TYPE_EVENT_DATA_BUFFER,
			.next = NULL,
		};
		r = xrPollEvent(xr.instance, &event);
		if (r != XR_EVENT_UNAVAILABLE) {
			if (XR_FAILED(r)) {
				wxrc_log_xr_result("xrPollEvent", r);
				return 1;
			}
			wxrc_xr_handle_event(&event, &running);
		}

		wl_display_flush_clients(wl_display);
		int ret = wl_event_loop_dispatch(wl_event_loop, 1);
		if (ret < 0) {
			wlr_log(WLR_ERROR, "wl_event_loop_dispatch failed");
			return 1;
		}

		if (!running) {
			break;
		}

		if (!wxrc_xr_push_frame(&xr, &gl, frame_state.predictedDisplayTime,
				xr_views, projection_views)) {
			return 1;
		}
	}

	wlr_log(WLR_DEBUG, "Tearing down XR instance");
	free(projection_views);
	free(xr_views);
	wl_event_source_remove(signals[0]);
	wl_event_source_remove(signals[1]);
	wxrc_gl_finish(&gl);
	wxrc_xr_finish(&xr);
	wl_display_destroy_clients(wl_display);
	wl_display_destroy(wl_display);
	return 0;
}
