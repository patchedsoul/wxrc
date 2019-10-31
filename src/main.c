#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <wayland-client.h>
#include <wlr/util/log.h>
#include <stdio.h>
#include <stdlib.h>
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
		XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
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
		XrSystemId sysid, XrSession *session) {
	/* TODO: We need to be more sophisticated about how we go about setting up
	 * the session's graphics binding */
	struct wl_display *display = wl_display_connect(NULL);
	if (!display) {
		wlr_log(WLR_ERROR, "Unable to connect to Wayland display");
		return XR_ERROR_INITIALIZATION_FAILED;
	}

	PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR;
	XrResult r = xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
		(PFN_xrVoidFunction *)&xrGetOpenGLGraphicsRequirementsKHR);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrGetInstanceProcAddr", r);
		return r;
	}

	XrGraphicsRequirementsOpenGLKHR reqs =
		{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
	r = xrGetOpenGLGraphicsRequirementsKHR(instance, sysid, &reqs);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrGetOpenGLGraphicsRequirementsKHR", r);
		return r;
	}

	wlr_log(WLR_DEBUG, "OpenGL gfx requirements: min %d.%d.%d, max: %d.%d.%d",
			XR_VERSION_MAJOR(reqs.minApiVersionSupported),
			XR_VERSION_MINOR(reqs.minApiVersionSupported),
			XR_VERSION_PATCH(reqs.minApiVersionSupported),
			XR_VERSION_MAJOR(reqs.maxApiVersionSupported),
			XR_VERSION_MINOR(reqs.maxApiVersionSupported),
			XR_VERSION_PATCH(reqs.maxApiVersionSupported));

	if (XR_VERSION_MAJOR(reqs.minApiVersionSupported) > 4 ||
			XR_VERSION_MAJOR(reqs.maxApiVersionSupported) < 4) {
		wlr_log(WLR_ERROR, "XR runtime does not support a suitable GL version.");
		return XR_ERROR_INITIALIZATION_FAILED;
	}

	XrGraphicsBindingOpenGLWaylandKHR gfx = {
		.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR,
		.display = display,
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

static void wxrc_xr_render_view(struct wxrc_xr_view *view, GLuint framebuffer,
		XrSwapchainImageOpenGLESKHR *image) {
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glViewport(0, 0, view->config.recommendedImageRectWidth,
		view->config.recommendedImageRectHeight);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		image->image, 0);

	glClearColor(0.0, 0.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static XrResult wxrc_xr_push_view_frame(struct wxrc_xr_view *view,
		XrView *xr_view, XrCompositionLayerProjectionView *projection_view) {
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

	wxrc_xr_render_view(view, view->framebuffers[buffer_index],
		&view->images[buffer_index]);

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

int main(int argc, char *argv[]) {
	wlr_log_init(WLR_DEBUG, NULL);

	if (XR_FAILED(wxrc_xr_enumerate_layer_props())) {
		return 1;
	}
	if (XR_FAILED(wxrc_xr_enumerate_instance_props())) {
		return 1;
	}

	XrInstance instance;
	XrResult r = wxrc_create_xr_instance(&instance);
	if (XR_FAILED(r)) {
		return 1;
	}

	XrSystemId sysid;
	r = wxrc_get_xr_system(instance, &sysid);
	if (XR_FAILED(r)) {
		return 1;
	}

	if (XR_FAILED(wxrc_xr_enumerate_view_configs(instance, sysid))) {
		return 1;
	}

	uint32_t nviews;
	XrViewConfigurationView *view_configs =
		wxrc_xr_enumerate_stereo_config_views(instance, sysid, &nviews);
	if (view_configs == NULL) {
		return 1;
	}

	XrSession session;
	r = wxrc_create_xr_session(instance, sysid, &session);
	if (XR_FAILED(r)) {
		return 1;
	}

	if (XR_FAILED(wxrc_xr_enumerate_reference_spaces(session))) {
		return 1;
	}

	XrSpace local_space;
	r = wxrc_xr_create_local_reference_space(session, &local_space);
	if (XR_FAILED(r)) {
		return 1;
	}

	wlr_log(WLR_DEBUG, "Starting XR session");
	XrSessionBeginInfo session_begin_info = {
		.type = XR_TYPE_SESSION_BEGIN_INFO,
		.next = NULL,
		.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
	};
	r = xrBeginSession(session, &session_begin_info);
	if (XR_FAILED(r)) {
		wxrc_log_xr_result("xrBeginSession", r);
		return 1;
	}

	struct wxrc_xr_view *views =
		wxrc_xr_create_swapchains(session, nviews, view_configs);
	if (views == NULL) {
		return 1;
	}

	XrView *xr_views = calloc(nviews, sizeof(XrView));
	XrCompositionLayerProjectionView *projection_views =
		calloc(nviews, sizeof(XrCompositionLayerProjectionView));
	while (1) {
		XrFrameWaitInfo frame_wait_info = {
			.type = XR_TYPE_FRAME_WAIT_INFO,
			.next = NULL,
		};
		XrFrameState frame_state = {
			.type = XR_TYPE_FRAME_STATE,
			.next = NULL,
		};
		r = xrWaitFrame(session, &frame_wait_info, &frame_state);
		if (XR_FAILED(r)) {
			wxrc_log_xr_result("xrWaitFrame", r);
			return 1;
		}

		for (uint32_t i = 0; i < nviews; i++) {
			xr_views[i].type = XR_TYPE_VIEW;
			xr_views[i].next = NULL;
		}

		XrViewLocateInfo view_locate_info = {
			.type = XR_TYPE_VIEW_LOCATE_INFO,
			.displayTime = frame_state.predictedDisplayTime,
			.space = local_space,
		};
		XrViewState view_state = {
			.type = XR_TYPE_VIEW_STATE,
			.next = NULL,
		};
		r = xrLocateViews(session, &view_locate_info, &view_state, nviews,
			&nviews, xr_views);
		if (XR_FAILED(r)) {
			wxrc_log_xr_result("xrLocateViews", r);
			return 1;
		}

		XrFrameBeginInfo frame_begin_info = {
			.type = XR_TYPE_FRAME_BEGIN_INFO,
			.next = NULL,
		};
		r = xrBeginFrame(session, &frame_begin_info);
		if (XR_FAILED(r)) {
			wxrc_log_xr_result("xrBeginFrame", r);
			return 1;
		}

		for (uint32_t i = 0; i < nviews; i++) {
			struct wxrc_xr_view *view = &views[i];
			wxrc_xr_push_view_frame(view, &xr_views[i], &projection_views[i]);
		}

		XrCompositionLayerProjection projection_layer = {
			.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
			.next = NULL,
			.layerFlags = 0,
			.space = local_space,
			.viewCount = nviews,
			.views = projection_views,
		};
		const XrCompositionLayerBaseHeader *projection_layers[] = {
			(XrCompositionLayerBaseHeader *)&projection_layer,
		};
		XrFrameEndInfo frame_end_info = {
			.type = XR_TYPE_FRAME_END_INFO,
			.displayTime = frame_state.predictedDisplayTime,
			.layerCount = 1,
			.layers = projection_layers,
			.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
			.next = NULL,
		};
		r = xrEndFrame(session, &frame_end_info);
		if (XR_FAILED(r)) {
			wxrc_log_xr_result("xrEndFrame", r);
			return 1;
		}
	}

	wlr_log(WLR_DEBUG, "Tearing down XR instance");
	free(projection_views);
	free(xr_views);
	free(views);
	free(view_configs);
	xrDestroyInstance(instance);
	return 0;
}
