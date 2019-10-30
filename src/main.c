#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <wayland-client.h>
#include <wlr/util/log.h>
#include <stdio.h>
#include <stdlib.h>
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
	XrViewConfigurationView *views =
		wxrc_xr_enumerate_stereo_config_views(instance, sysid, &nviews);
	if (views == NULL) {
		return 1;
	}

	XrSession session;
	r = wxrc_create_xr_session(instance, sysid, &session);
	if (XR_FAILED(r)) {
		return 1;
	}

	wlr_log(WLR_DEBUG, "Tearing down XR instance");
	free(views);
	xrDestroyInstance(instance);
	return 0;
}
