#include <cglm/cglm.h>
#include <math.h>
#include <openxr/openxr.h>
#include <stdio.h>
#include <wlr/util/log.h>
#include "xrutil.h"

const char *wxrc_xr_result_str(XrResult r) {
	static char reason[64];
	switch (r) {
	case XR_SUCCESS:
		return "XR_SUCCESS";
	case XR_TIMEOUT_EXPIRED:
		return "XR_TIMEOUT_EXPIRED";
	case XR_SESSION_LOSS_PENDING:
		return "XR_SESSION_LOSS_PENDING";
	case XR_EVENT_UNAVAILABLE:
		return "XR_EVENT_UNAVAILABLE";
	case XR_SPACE_BOUNDS_UNAVAILABLE:
		return "XR_SPACE_BOUNDS_UNAVAILABLE";
	case XR_SESSION_NOT_FOCUSED:
		return "XR_SESSION_NOT_FOCUSED";
	case XR_FRAME_DISCARDED:
		return "XR_FRAME_DISCARDED";
	case XR_ERROR_VALIDATION_FAILURE:
		return "XR_ERROR_VALIDATION_FAILURE";
	case XR_ERROR_RUNTIME_FAILURE:
		return "XR_ERROR_RUNTIME_FAILURE";
	case XR_ERROR_OUT_OF_MEMORY:
		return "XR_ERROR_OUT_OF_MEMORY";
	case XR_ERROR_API_VERSION_UNSUPPORTED:
		return "XR_ERROR_API_VERSION_UNSUPPORTED";
	case XR_ERROR_INITIALIZATION_FAILED:
		return "XR_ERROR_INITIALIZATION_FAILED";
	case XR_ERROR_FUNCTION_UNSUPPORTED:
		return "XR_ERROR_FUNCTION_UNSUPPORTED";
	case XR_ERROR_FEATURE_UNSUPPORTED:
		return "XR_ERROR_FEATURE_UNSUPPORTED";
	case XR_ERROR_EXTENSION_NOT_PRESENT:
		return "XR_ERROR_EXTENSION_NOT_PRESENT";
	case XR_ERROR_LIMIT_REACHED:
		return "XR_ERROR_LIMIT_REACHED";
	case XR_ERROR_SIZE_INSUFFICIENT:
		return "XR_ERROR_SIZE_INSUFFICIENT";
	case XR_ERROR_HANDLE_INVALID:
		return "XR_ERROR_HANDLE_INVALID";
	case XR_ERROR_INSTANCE_LOST:
		return "XR_ERROR_INSTANCE_LOST";
	case XR_ERROR_SESSION_RUNNING:
		return "XR_ERROR_SESSION_RUNNING";
	case XR_ERROR_SESSION_NOT_RUNNING:
		return "XR_ERROR_SESSION_NOT_RUNNING";
	case XR_ERROR_SESSION_LOST:
		return "XR_ERROR_SESSION_LOST";
	case XR_ERROR_SYSTEM_INVALID:
		return "XR_ERROR_SYSTEM_INVALID";
	case XR_ERROR_PATH_INVALID:
		return "XR_ERROR_PATH_INVALID";
	case XR_ERROR_PATH_COUNT_EXCEEDED:
		return "XR_ERROR_PATH_COUNT_EXCEEDED";
	case XR_ERROR_PATH_FORMAT_INVALID:
		return "XR_ERROR_PATH_FORMAT_INVALID";
	case XR_ERROR_PATH_UNSUPPORTED:
		return "XR_ERROR_PATH_UNSUPPORTED";
	case XR_ERROR_LAYER_INVALID:
		return "XR_ERROR_LAYER_INVALID";
	case XR_ERROR_LAYER_LIMIT_EXCEEDED:
		return "XR_ERROR_LAYER_LIMIT_EXCEEDED";
	case XR_ERROR_SWAPCHAIN_RECT_INVALID:
		return "XR_ERROR_SWAPCHAIN_RECT_INVALID";
	case XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED:
		return "XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED";
	case XR_ERROR_ACTION_TYPE_MISMATCH:
		return "XR_ERROR_ACTION_TYPE_MISMATCH";
	case XR_ERROR_SESSION_NOT_READY:
		return "XR_ERROR_SESSION_NOT_READY";
	case XR_ERROR_SESSION_NOT_STOPPING:
		return "XR_ERROR_SESSION_NOT_STOPPING";
	case XR_ERROR_TIME_INVALID:
		return "XR_ERROR_TIME_INVALID";
	case XR_ERROR_REFERENCE_SPACE_UNSUPPORTED:
		return "XR_ERROR_REFERENCE_SPACE_UNSUPPORTED";
	case XR_ERROR_FILE_ACCESS_ERROR:
		return "XR_ERROR_FILE_ACCESS_ERROR";
	case XR_ERROR_FILE_CONTENTS_INVALID:
		return "XR_ERROR_FILE_CONTENTS_INVALID";
	case XR_ERROR_FORM_FACTOR_UNSUPPORTED:
		return "XR_ERROR_FORM_FACTOR_UNSUPPORTED";
	case XR_ERROR_FORM_FACTOR_UNAVAILABLE:
		return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
	case XR_ERROR_API_LAYER_NOT_PRESENT:
		return "XR_ERROR_API_LAYER_NOT_PRESENT";
	case XR_ERROR_CALL_ORDER_INVALID:
		return "XR_ERROR_CALL_ORDER_INVALID";
	case XR_ERROR_GRAPHICS_DEVICE_INVALID:
		return "XR_ERROR_GRAPHICS_DEVICE_INVALID";
	case XR_ERROR_POSE_INVALID:
		return "XR_ERROR_POSE_INVALID";
	case XR_ERROR_INDEX_OUT_OF_RANGE:
		return "XR_ERROR_INDEX_OUT_OF_RANGE";
	case XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED:
		return "XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED";
	case XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED:
		return "XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED";
	case XR_ERROR_NAME_DUPLICATED:
		return "XR_ERROR_NAME_DUPLICATED";
	case XR_ERROR_NAME_INVALID:
		return "XR_ERROR_NAME_INVALID";
	case XR_ERROR_ACTIONSET_NOT_ATTACHED:
		return "XR_ERROR_ACTIONSET_NOT_ATTACHED";
	case XR_ERROR_ACTIONSETS_ALREADY_ATTACHED:
		return "XR_ERROR_ACTIONSETS_ALREADY_ATTACHED";
	case XR_ERROR_LOCALIZED_NAME_DUPLICATED:
		return "XR_ERROR_LOCALIZED_NAME_DUPLICATED";
	case XR_ERROR_LOCALIZED_NAME_INVALID:
		return "XR_ERROR_LOCALIZED_NAME_INVALID";
	case XR_ERROR_ANDROID_THREAD_SETTINGS_ID_INVALID_KHR:
		return "XR_ERROR_ANDROID_THREAD_SETTINGS_ID_INVALID_KHR";
	case XR_ERROR_ANDROID_THREAD_SETTINGS_FAILURE_KHR:
		return "XR_ERROR_ANDROID_THREAD_SETTINGS_FAILURE_KHR";
	case XR_ERROR_CREATE_SPATIAL_ANCHOR_FAILED_MSFT:
		return "XR_ERROR_CREATE_SPATIAL_ANCHOR_FAILED_MSFT";
	default:
		sprintf(reason, "Unknown XR result %d", (int)r);
		return reason;
	}
}

const char *wxrc_xr_structure_type_str(XrStructureType t) {
	static char type[64];
	switch (t) {
	case XR_TYPE_UNKNOWN:
		return "XR_TYPE_UNKNOWN";
	case XR_TYPE_API_LAYER_PROPERTIES:
		return "XR_TYPE_API_LAYER_PROPERTIES";
	case XR_TYPE_EXTENSION_PROPERTIES:
		return "XR_TYPE_EXTENSION_PROPERTIES";
	case XR_TYPE_INSTANCE_CREATE_INFO:
		return "XR_TYPE_INSTANCE_CREATE_INFO";
	case XR_TYPE_SYSTEM_GET_INFO:
		return "XR_TYPE_SYSTEM_GET_INFO";
	case XR_TYPE_SYSTEM_PROPERTIES:
		return "XR_TYPE_SYSTEM_PROPERTIES";
	case XR_TYPE_VIEW_LOCATE_INFO:
		return "XR_TYPE_VIEW_LOCATE_INFO";
	case XR_TYPE_VIEW:
		return "XR_TYPE_VIEW";
	case XR_TYPE_SESSION_CREATE_INFO:
		return "XR_TYPE_SESSION_CREATE_INFO";
	case XR_TYPE_SWAPCHAIN_CREATE_INFO:
		return "XR_TYPE_SWAPCHAIN_CREATE_INFO";
	case XR_TYPE_SESSION_BEGIN_INFO:
		return "XR_TYPE_SESSION_BEGIN_INFO";
	case XR_TYPE_VIEW_STATE:
		return "XR_TYPE_VIEW_STATE";
	case XR_TYPE_FRAME_END_INFO:
		return "XR_TYPE_FRAME_END_INFO";
	case XR_TYPE_HAPTIC_VIBRATION:
		return "XR_TYPE_HAPTIC_VIBRATION";
	case XR_TYPE_EVENT_DATA_BUFFER:
		return "XR_TYPE_EVENT_DATA_BUFFER";
	case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
		return "XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING";
	case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		return "XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED";
	case XR_TYPE_ACTION_STATE_BOOLEAN:
		return "XR_TYPE_ACTION_STATE_BOOLEAN";
	case XR_TYPE_ACTION_STATE_FLOAT:
		return "XR_TYPE_ACTION_STATE_FLOAT";
	case XR_TYPE_ACTION_STATE_VECTOR2F:
		return "XR_TYPE_ACTION_STATE_VECTOR2F";
	case XR_TYPE_ACTION_STATE_POSE:
		return "XR_TYPE_ACTION_STATE_POSE";
	case XR_TYPE_ACTION_SET_CREATE_INFO:
		return "XR_TYPE_ACTION_SET_CREATE_INFO";
	case XR_TYPE_ACTION_CREATE_INFO:
		return "XR_TYPE_ACTION_CREATE_INFO";
	case XR_TYPE_INSTANCE_PROPERTIES:
		return "XR_TYPE_INSTANCE_PROPERTIES";
	case XR_TYPE_FRAME_WAIT_INFO:
		return "XR_TYPE_FRAME_WAIT_INFO";
	case XR_TYPE_COMPOSITION_LAYER_PROJECTION:
		return "XR_TYPE_COMPOSITION_LAYER_PROJECTION";
	case XR_TYPE_COMPOSITION_LAYER_QUAD:
		return "XR_TYPE_COMPOSITION_LAYER_QUAD";
	case XR_TYPE_REFERENCE_SPACE_CREATE_INFO:
		return "XR_TYPE_REFERENCE_SPACE_CREATE_INFO";
	case XR_TYPE_ACTION_SPACE_CREATE_INFO:
		return "XR_TYPE_ACTION_SPACE_CREATE_INFO";
	case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
		return "XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING";
	case XR_TYPE_VIEW_CONFIGURATION_VIEW:
		return "XR_TYPE_VIEW_CONFIGURATION_VIEW";
	case XR_TYPE_SPACE_LOCATION:
		return "XR_TYPE_SPACE_LOCATION";
	case XR_TYPE_SPACE_VELOCITY:
		return "XR_TYPE_SPACE_VELOCITY";
	case XR_TYPE_FRAME_STATE:
		return "XR_TYPE_FRAME_STATE";
	case XR_TYPE_VIEW_CONFIGURATION_PROPERTIES:
		return "XR_TYPE_VIEW_CONFIGURATION_PROPERTIES";
	case XR_TYPE_FRAME_BEGIN_INFO:
		return "XR_TYPE_FRAME_BEGIN_INFO";
	case XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW:
		return "XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW";
	case XR_TYPE_EVENT_DATA_EVENTS_LOST:
		return "XR_TYPE_EVENT_DATA_EVENTS_LOST";
	case XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING:
		return "XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING";
	case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
		return "XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED";
	case XR_TYPE_INTERACTION_PROFILE_STATE:
		return "XR_TYPE_INTERACTION_PROFILE_STATE";
	case XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO:
		return "XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO";
	case XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO:
		return "XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO";
	case XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO:
		return "XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO";
	case XR_TYPE_ACTION_STATE_GET_INFO:
		return "XR_TYPE_ACTION_STATE_GET_INFO";
	case XR_TYPE_HAPTIC_ACTION_INFO:
		return "XR_TYPE_HAPTIC_ACTION_INFO";
	case XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO:
		return "XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO";
	case XR_TYPE_ACTIONS_SYNC_INFO:
		return "XR_TYPE_ACTIONS_SYNC_INFO";
	case XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO:
		return "XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO";
	case XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO:
		return "XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO";
	case XR_TYPE_COMPOSITION_LAYER_CUBE_KHR:
		return "XR_TYPE_COMPOSITION_LAYER_CUBE_KHR";
	case XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR:
		return "XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR";
	case XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR:
		return "XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR";
	case XR_TYPE_VULKAN_SWAPCHAIN_FORMAT_LIST_CREATE_INFO_KHR:
		return "XR_TYPE_VULKAN_SWAPCHAIN_FORMAT_LIST_CREATE_INFO_KHR";
	case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT:
		return "XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT";
	case XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR:
		return "XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR";
	case XR_TYPE_COMPOSITION_LAYER_EQUIRECT_KHR:
		return "XR_TYPE_COMPOSITION_LAYER_EQUIRECT_KHR";
	case XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT:
		return "XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT";
	case XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT:
		return "XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT";
	case XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT:
		return "XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT";
	case XR_TYPE_DEBUG_UTILS_LABEL_EXT:
		return "XR_TYPE_DEBUG_UTILS_LABEL_EXT";
	case XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR:
		return "XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR";
	case XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR:
		return "XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR";
	case XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR:
		return "XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR";
	case XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR:
		return "XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR";
	case XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR:
		return "XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR";
	case XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR:
		return "XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR";
	case XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR:
		return "XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR";
	case XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR:
		return "XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR";
	case XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR:
		return "XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR";
	case XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR:
		return "XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR";
	case XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR:
		return "XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR";
	case XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR:
		return "XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR";
	case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR:
		return "XR_TYPE_GRAPHICS_BINDING_D3D11_KHR";
	case XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR:
		return "XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR";
	case XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR:
		return "XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR";
	case XR_TYPE_GRAPHICS_BINDING_D3D12_KHR:
		return "XR_TYPE_GRAPHICS_BINDING_D3D12_KHR";
	case XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR:
		return "XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR";
	case XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR:
		return "XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR";
	case XR_TYPE_VISIBILITY_MASK_KHR:
		return "XR_TYPE_VISIBILITY_MASK_KHR";
	case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR:
		return "XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR";
	case XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_MSFT:
		return "XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_MSFT";
	case XR_TYPE_SPATIAL_ANCHOR_SPACE_CREATE_INFO_MSFT:
		return "XR_TYPE_SPATIAL_ANCHOR_SPACE_CREATE_INFO_MSFT";
	default:
		sprintf(type, "Unknown XR structure type %d", (int)t);
		return type;
	}
}

void wxrc_log_xr_result(const char *entrypoint, XrResult r) {
	wlr_log(XR_FAILED(r) ? WLR_ERROR : WLR_DEBUG,
			"%s: %s", entrypoint, wxrc_xr_result_str(r));
}

void wxrc_xr_projection_from_fov(const XrFovf *fov, float near_z, float far_z,
		mat4 dest) {
	float tan_left = tanf(fov->angleLeft);
	float tan_right = tanf(fov->angleRight);

	float tan_down = tanf(fov->angleDown);
	float tan_up = tanf(fov->angleUp);

	float tan_width = tan_right - tan_left;
	float tan_height = tan_up - tan_down;

	float a11 = 2 / tan_width;
	float a22 = 2 / tan_height;

	float a31 = (tan_right + tan_left) / tan_width;
	float a32 = (tan_up + tan_down) / tan_height;
	float a33 = -far_z / (far_z - near_z);

	float a43 = -(far_z * near_z) / (far_z - near_z);

	const mat4 mat = {
		a11, 0, 0, 0, 0, a22, 0, 0, a31, a32, a33, -1, 0, 0, a43, 0,
	};

	memcpy(dest, mat, sizeof(mat));
}

void wxrc_xr_vector3f_to_cglm(const XrVector3f *in, vec3 out) {
	out[0] = in->x;
	out[1] = in->y;
	out[2] = in->z;
}

void wxrc_xr_quaternion_to_cglm(const XrQuaternionf *in, versor out) {
	out[0] = in->x;
	out[1] = in->y;
	out[2] = in->z;
	out[3] = in->w;
}
