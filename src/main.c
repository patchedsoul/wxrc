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
				prop->layerName, prop->specVersion, prop->layerVersion,
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
		wlr_log(WLR_DEBUG, "%s v%d", prop->extensionName,
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

XrResult wxrc_create_xr_session(XrInstance instance,
		XrSystemId sysid, XrSession *session) {
	/* TODO: We need to be more sophisticated about how we go about setting up
	 * the session's graphics binding */
	struct wl_display *display = wl_display_connect(NULL);
	if (!display) {
		wlr_log(WLR_ERROR, "Unable to connect to Wayland display");
		return XR_ERROR_INITIALIZATION_FAILED;
	}

	XrGraphicsRequirementsOpenGLKHR reqs =
		{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
	XrResult r = xrGetOpenGLGraphicsRequirementsKHR(instance, sysid, &reqs);
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

	XrSession session;
	r = wxrc_create_xr_session(instance, sysid, &session);

	wlr_log(WLR_DEBUG, "Tearing down XR instance");
	xrDestroyInstance(instance);
	return 0;
}
