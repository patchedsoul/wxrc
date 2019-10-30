#ifndef _WXRC_COMPOSITOR_H
#define _WXRC_COMPOSITOR_H

#include <GLES2/gl2.h>
#include <openxr/openxr.h>

struct wxrc_xr_view {
	XrViewConfigurationView view;
	XrSwapchain swapchain;

	uint32_t nimages;
	XrSwapchainImageOpenGLESKHR *images;
	GLuint *framebuffers;
};

#endif
