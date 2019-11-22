#include <drm_fourcc.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <assert.h>
#include <cglm/cglm.h>
#include <fcntl.h>
#include <gbm.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "zxr-shell-unstable-v1-client-protocol.h"

static const size_t nvertices = 12 * 3; /* 6 faces → 12 triangles */
static const GLfloat vertices[] = {
    -1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    1.0f, 1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f,-1.0f,
    1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f,-1.0f,
    1.0f,-1.0f,-1.0f,
    1.0f, 1.0f,-1.0f,
    1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
    1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,
    1.0f,-1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f,-1.0f,-1.0f,
    1.0f, 1.0f,-1.0f,
    1.0f,-1.0f,-1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f,-1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f,-1.0f,
    -1.0f, 1.0f,-1.0f,
    1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    1.0f,-1.0f, 1.0f,
};

static const GLchar vertex_shader_src[] =
	"#version 100\n"
	"\n"
	"attribute vec3 pos;\n"
	"uniform mat4 mvp;\n"
	"\n"
	"varying vec3 vertex_pos;\n"
	"\n"
	"void main() {\n"
	"	gl_Position = mvp * vec4(pos, 1.0);\n"
	"	vertex_pos = pos;\n"
	"}\n";

static const GLchar fragment_shader_src[] =
	"#version 100\n"
	"precision mediump float;\n"
	"\n"
	"varying vec3 vertex_pos;\n"
	"\n"
	"void main() {\n"
	"	int n = 0;\n"
	"	float thr = 0.01;\n"
	"	if (1.0 - abs(vertex_pos.x) < thr) n++;\n"
	"	if (1.0 - abs(vertex_pos.y) < thr) n++;\n"
	"	if (1.0 - abs(vertex_pos.z) < thr) n++;\n"
	"	if (n >= 2) {\n"
	"		gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
	"	} else {\n"
	"		discard;\n"
	"	}\n"
	"}\n";

/* TODO: Don't hardcode these */
static int display_width = 2160;
static int display_height = 1200;

static bool running = true;

static struct wl_compositor *compositor;
static struct zwp_linux_dmabuf_v1 *linux_dmabuf;
static struct zxr_shell_v1 *xr_shell;

static struct wl_surface *surface;
static struct zxr_surface_v1 *xr_surface;
static struct zxr_composite_buffer_v1 *composite_buffer;
static struct wl_buffer *composite_wl_buffer;

#define MAX_BUFFER_PLANES 4
#define BUFFERS_PER_VIEW 3

struct cube_buffer {
	struct gbm_bo *bo;

	int width, height;
	int format;
	uint64_t modifier;
	int nplanes;
	int dmabuf_fds[MAX_BUFFER_PLANES];
	uint32_t strides[MAX_BUFFER_PLANES];
	uint32_t offsets[MAX_BUFFER_PLANES];

	struct wl_buffer *buffer;
	bool busy;

	EGLImageKHR egl_image;
	GLuint gl_texture;
	GLuint gl_fbo;
};

struct cube_xr_view {
	struct zxr_view_v1 *view;
	struct zxr_surface_view_v1 *surface_view;
	struct cube_buffer buffers[BUFFERS_PER_VIEW];
	mat4 mvp_matrix;
	struct wl_list link;
};

static struct wl_list xr_views;

static struct {
	EGLDisplay display;
	EGLContext context;

	PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
	PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
	PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

	/* TODO: query modifiers
	PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT;
	*/

	/* TODO: Explicit sync
	PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
	PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
	PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
	PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
	PFNEGLWAITSYNCKHRPROC eglClientWaitSyncKHR;
	*/
} egl;

static struct {
	int drm_fd;
	struct gbm_device *device;
} gbm;

static GLuint gl_prog;

static void render(uint32_t time);

static struct cube_buffer *view_next_buffer(struct cube_xr_view *view) {
	for (int i = 0; i < BUFFERS_PER_VIEW; i++) {
		if (!view->buffers[i].busy) {
			return &view->buffers[i];
		}
	}
	return NULL;
}

static void frame_handle_done(void *data,
		struct wl_callback *callback, uint32_t time) {
	wl_callback_destroy(callback);
	render(time);
}

static const struct wl_callback_listener frame_listener = {
	.done = frame_handle_done,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		compositor = wl_registry_bind(
				registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, zxr_shell_v1_interface.name) == 0) {
		xr_shell = wl_registry_bind(registry, name, &zxr_shell_v1_interface, 1);
	} else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0) {
		linux_dmabuf = wl_registry_bind(registry, name,
				&zwp_linux_dmabuf_v1_interface, 1);
	} else if (strcmp(interface, zxr_view_v1_interface.name) == 0) {
		struct cube_xr_view *view = calloc(1, sizeof(struct cube_xr_view));
		assert(view);
		view->view = wl_registry_bind(registry, name,
				&zxr_view_v1_interface, 1);
		wl_list_insert(&xr_views, &view->link);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// Who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static GLuint compile_shader(GLuint type, const GLchar *src) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		fprintf(stderr, "Failed to compile shader: %s\n", log);
		return 0;
	}

	return shader;
}

static GLuint compile_program(void) {
	GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
	if (vertex_shader == 0) {
		fprintf(stderr, "Failed to compile vertex shader\n");
		return 0;
	}

	GLuint fragment_shader =
		compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
	if (fragment_shader == 0) {
		fprintf(stderr, "Failed to compile fragment shader\n");
		return 0;
	}

	GLuint shader_program = glCreateProgram();
	glAttachShader(shader_program, vertex_shader);
	glAttachShader(shader_program, fragment_shader);
	glLinkProgram(shader_program);
	GLint ok;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetProgramInfoLog(shader_program, sizeof(log), NULL, log);
		fprintf(stderr, "Failed to link program: %s\n", log);
		return 0;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return shader_program;
}

static void render_scene(uint32_t time, mat4 mvp_matrix) {
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	GLint pos_loc = glGetAttribLocation(gl_prog, "pos");
	GLint mvp_loc = glGetUniformLocation(gl_prog, "mvp");

	glUseProgram(gl_prog);

	glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (GLfloat *)mvp_matrix);

	GLint coords_per_vertex =
		sizeof(vertices) / sizeof(vertices[0]) / nvertices;
	glVertexAttribPointer(pos_loc, coords_per_vertex,
		GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(pos_loc);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, nvertices);

	glDisableVertexAttribArray(pos_loc);

	glUseProgram(0);
}

static void render_view(struct cube_xr_view *view, uint32_t time) {
	struct cube_buffer *buffer = view_next_buffer(view);
	assert(buffer);

	glBindFramebuffer(GL_FRAMEBUFFER, buffer->gl_fbo);
	glViewport(0, 0, display_width, display_height);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	mat4 model_matrix = GLM_MAT4_IDENTITY_INIT;
	glm_rotate_y(model_matrix, (float)time / 1000, model_matrix);
	glm_scale_uni(model_matrix, 0.3);

	mat4 mvp_matrix = GLM_MAT4_IDENTITY_INIT;
	glm_mat4_mul(view->mvp_matrix, model_matrix, mvp_matrix);
	render_scene(time, mvp_matrix);

	glFinish();

	zxr_composite_buffer_v1_attach_buffer(
			composite_buffer, view->view, buffer->buffer,
			ZXR_COMPOSITE_BUFFER_V1_BUFFER_TYPE_PIXEL_BUFFER);
}

static void render(uint32_t time) {
	struct cube_xr_view *view;
	wl_list_for_each(view, &xr_views, link) {
		render_view(view, time);
	}

	struct wl_callback *callback = wl_surface_frame(surface);
	wl_callback_add_listener(callback, &frame_listener, NULL);

	wl_surface_attach(surface, composite_wl_buffer, 0, 0);
	wl_surface_damage_buffer(surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(surface);
}

static void buffer_release(void *data, struct wl_buffer *buffer) {
	struct cube_buffer *buf = data;
	buf->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_release,
};

static void linux_dmabuf_created(void *data,
		struct zwp_linux_buffer_params_v1 *params,
		struct wl_buffer *new_buffer) {
	struct cube_buffer *buf = data;
	buf->buffer = new_buffer;
	/* TODO: Explicit sync */
	wl_buffer_add_listener(buf->buffer, &buffer_listener, buf);
	zwp_linux_buffer_params_v1_destroy(params);
}

static void linux_dmabuf_failed(void *data,
		struct zwp_linux_buffer_params_v1 *params) {
	assert(false && "dmabuf creation failed");
}

static const struct zwp_linux_buffer_params_v1_listener params_listener = {
	.created = linux_dmabuf_created,
	.failed = linux_dmabuf_failed,
};

static void allocate_buffer(struct cube_buffer *buffer) {
	static const uint32_t flags = ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT;

	/* TODO: Do not hard code this */
	buffer->width = display_width, buffer->height = display_height;

	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->bo = gbm_bo_create(gbm.device, buffer->width, buffer->height,
			buffer->format, GBM_BO_USE_RENDERING);
	assert(buffer->bo);

	/* TODO: Planes */
	buffer->nplanes = 0;
	buffer->modifier = DRM_FORMAT_MOD_INVALID;
	buffer->dmabuf_fds[0] = gbm_bo_get_fd(buffer->bo);
	buffer->strides[0] = gbm_bo_get_stride(buffer->bo);
	buffer->offsets[0] = 0;

	struct zwp_linux_buffer_params_v1 *params =
		zwp_linux_dmabuf_v1_create_params(linux_dmabuf);
	zwp_linux_buffer_params_v1_add(params,
		buffer->dmabuf_fds[0], 0, buffer->offsets[0], buffer->strides[0],
		buffer->modifier >> 32, buffer->modifier & 0xFFFFFFFF);
	zwp_linux_buffer_params_v1_add_listener(params, &params_listener, buffer);
	zwp_linux_buffer_params_v1_create(params,
		buffer->width, buffer->height, buffer->format, flags);

	static const int general_attribs = 3;
	static const int plane_attribs = 5;
	static const int entries_per_attrib = 2;
	EGLint attribs[(general_attribs + plane_attribs * MAX_BUFFER_PLANES) *
			entries_per_attrib + 1];
	unsigned int atti = 0;
	attribs[atti++] = EGL_WIDTH;
	attribs[atti++] = buffer->width;
	attribs[atti++] = EGL_HEIGHT;
	attribs[atti++] = buffer->height;
	attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
	attribs[atti++] = buffer->format;
	/* plane 0 */
	attribs[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
	attribs[atti++] = buffer->dmabuf_fds[0];
	attribs[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
	attribs[atti++] = (int)buffer->offsets[0];
	attribs[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
	attribs[atti++] = (int)buffer->strides[0];
	/* TODO: Update for dmabuf import modifiers */
	attribs[atti] = EGL_NONE;
	assert(atti < sizeof(attribs) / sizeof(attribs[0]));

	buffer->egl_image = egl.eglCreateImageKHR(egl.display,
		EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
	assert(buffer->egl_image != EGL_NO_IMAGE_KHR);

	eglMakeCurrent(egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl.context);

	glGenTextures(1, &buffer->gl_texture);
	glBindTexture(GL_TEXTURE_2D, buffer->gl_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	egl.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, buffer->egl_image);
	glGenFramebuffers(1, &buffer->gl_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, buffer->gl_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			buffer->gl_texture, 0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

static void surface_view_handle_mvp_matrix(void *data,
		struct zxr_surface_view_v1 *surface_view, struct wl_array *matrix) {
	struct cube_xr_view *view = data;
	assert(matrix->size == sizeof(mat4));
	memcpy(view->mvp_matrix, matrix->data, matrix->size);
}

static const struct zxr_surface_view_v1_listener surface_view_listener = {
	.mvp_matrix = surface_view_handle_mvp_matrix,
};

int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return 1;
	}

	egl.eglGetPlatformDisplayEXT =
		(void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
	egl.eglCreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
	egl.eglDestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
	egl.glEGLImageTargetTexture2DOES =
		(void *)eglGetProcAddress("glEGLImageTargetTexture2DOES");

	assert(egl.eglGetPlatformDisplayEXT && egl.eglCreateImageKHR
			&& egl.eglDestroyImageKHR && egl.glEGLImageTargetTexture2DOES);

	wl_list_init(&xr_views);

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);
	assert(compositor && xr_shell && linux_dmabuf);

	char const *drm_render_node = "/dev/dri/renderD128"; // configurable?
	gbm.drm_fd = open(drm_render_node, O_RDWR);
	assert(gbm.drm_fd >= 0);
	gbm.device = gbm_create_device(gbm.drm_fd);
	assert(gbm.device);

	egl.display = egl.eglGetPlatformDisplayEXT(
			EGL_PLATFORM_GBM_KHR, gbm.device, NULL);
	assert(egl.display);

	EGLint major, minor;
	if (!eglInitialize(egl.display, &major, &minor)) {
		fprintf(stderr, "failed to initialize EGL\n");
		return 1;
	}

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE,
	};
	EGLint n = 0;
	EGLConfig egl_config;
	eglChooseConfig(egl.display, config_attribs, &egl_config, 1, &n);
	if (n == 0) {
		fprintf(stderr, "failed to choose an EGL config\n");
		return 1;
	}

	EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE,
	};
	egl.context = eglCreateContext(egl.display, egl_config,
		EGL_NO_CONTEXT, context_attribs);
	assert(egl.context != EGL_NO_CONTEXT);

	surface = wl_compositor_create_surface(compositor);
	xr_surface = zxr_shell_v1_get_xr_surface(xr_shell, surface);

	/* TODO: Handle view addition/removal at runtime */
	struct cube_xr_view *view;
	wl_list_for_each(view, &xr_views, link) {
		view->surface_view = zxr_surface_v1_get_surface_view(
				xr_surface, view->view);
		zxr_surface_view_v1_add_listener(view->surface_view,
				&surface_view_listener, view);
		for (int i = 0; i < BUFFERS_PER_VIEW; ++i) {
			allocate_buffer(&view->buffers[i]);
		}
	}
	composite_buffer = zxr_shell_v1_create_composite_buffer(xr_shell);
	composite_wl_buffer = zxr_composite_buffer_v1_get_wl_buffer(composite_buffer);

	wl_surface_commit(surface);
	wl_display_roundtrip(display);

	eglSwapInterval(egl.display, 0);

	gl_prog = compile_program();
	if (gl_prog == 0) {
		fprintf(stderr, "Failed to compile shader program\n");
		return 1;
	}

	eglMakeCurrent(egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl.context);

	render(0);

	while (wl_display_dispatch(display) != -1 && running) {
		// This space intentionally left blank
	}

	/* TODO: Clean up resources */
	return 0;
}
