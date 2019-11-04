#ifndef _WXRC_COMPOSITOR_H
#define _WXRC_COMPOSITOR_H

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

struct wxrc_xr_view {
	XrViewConfigurationView config;
	XrSwapchain swapchain;

	uint32_t nimages;
	XrSwapchainImageOpenGLESKHR *images;
	GLuint *framebuffers;
};

struct wxrc_xr {
	XrInstance instance;
	XrSession session;

	XrSpace local_space;

	uint32_t nviews;
	struct wxrc_xr_view *views;
};

#endif
