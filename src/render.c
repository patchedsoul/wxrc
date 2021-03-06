#include <cglm/cglm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/render/gles2.h>
#include "mathutil.h"
#include "render.h"
#include "server.h"
#include "backend.h"
#include "view.h"
#include "xrutil.h"

static const GLchar grid_vertex_shader_src[] =
	"#version 100\n"
	"\n"
	"attribute vec3 pos;\n"
	"uniform mat4 mvp;\n"
	"\n"
	"varying vec3 vertex_pos;\n"
	"\n"
	"void main() {\n"
	"	vertex_pos = pos;\n"
	"	gl_Position = mvp * vec4(pos, 1.0);\n"
	"}\n";

static const GLchar grid_fragment_shader_src[] =
	"#version 100\n"
	"precision mediump float;\n"
	"\n"
	"uniform vec4 fg_color;\n"
	"\n"
	"varying vec3 vertex_pos;\n"
	"\n"
	"void main() {\n"
	"	if (fract(vertex_pos.x * 100.0) < 0.01 ||\n"
	"			fract(vertex_pos.y * 100.0) < 0.01) {\n"
	"		float a = distance(vertex_pos, vec3(0)) * 7.0;\n"
	"		a = min(a, 1.0);\n"
	"		gl_FragColor = mix(fg_color, vec4(0), a);\n"
	"	} else {\n"
	"		discard;\n"
	"	}\n"
	"}\n";

static const GLchar texture_vertex_shader_src[] =
	"#version 100\n"
	"\n"
	"attribute vec2 tex_coord;\n"
	"uniform mat4 mvp;\n"
	"uniform bool invert_y;\n"
	"\n"
	"varying vec2 vertex_tex_coord;\n"
	"\n"
	"void main() {\n"
	"	vertex_tex_coord = tex_coord;\n"
	"	if (invert_y) {\n"
	"		vertex_tex_coord.y = 1.0 - vertex_tex_coord.y;\n"
	"	}\n"
	"	gl_Position = mvp * vec4(tex_coord, 0.0, 1.0);\n"
	"}\n";

static const GLchar texture_rgb_fragment_shader_src[] =
	"#version 100\n"
	"precision mediump float;\n"
	"\n"
	"uniform sampler2D tex;\n"
	"uniform bool has_alpha;\n"
	"\n"
	"varying vec2 vertex_tex_coord;\n"
	"\n"
	"void main() {\n"
	"	gl_FragColor = texture2D(tex, vertex_tex_coord);\n"
	"	if (!has_alpha) {\n"
	"		gl_FragColor.a = 1.0;\n"
	"	}\n"
	"}\n";

static const GLchar texture_external_fragment_shader_src[] =
	"#version 100\n"
	"#extension GL_OES_EGL_image_external : require\n"
	"precision mediump float;\n"
	"\n"
	"uniform samplerExternalOES tex;\n"
	"\n"
	"varying vec2 vertex_tex_coord;\n"
	"\n"
	"void main() {\n"
	"	gl_FragColor = texture2D(tex, vertex_tex_coord);\n"
	"}\n";

static GLuint wxrc_gl_compile_shader(GLuint type, const GLchar *src) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		wlr_log(WLR_ERROR, "Failed to compile shader: %s", log);
		return 0;
	}

	return shader;
}

struct wxrc_shader_build_job {
	const char *name;
	const GLchar *vertex_src;
	const GLchar *fragment_src;
	GLuint *program_ptr;
};

bool wxrc_gl_init(struct wxrc_gl *gl) {
	struct wxrc_shader_build_job jobs[] = {
		{
			.name = "grid",
			.vertex_src = grid_vertex_shader_src,
			.fragment_src = grid_fragment_shader_src,
			.program_ptr = &gl->grid_program,
		},
		{
			.name = "texture_rgb",
			.vertex_src = texture_vertex_shader_src,
			.fragment_src = texture_rgb_fragment_shader_src,
			.program_ptr = &gl->texture_rgb_program,
		},
		{
			.name = "texture_external",
			.vertex_src = texture_vertex_shader_src,
			.fragment_src = texture_external_fragment_shader_src,
			.program_ptr = &gl->texture_external_program,
		},
	};

	for (size_t i = 0; i < sizeof(jobs) / sizeof(jobs[0]); i++) {
		struct wxrc_shader_build_job *job = &jobs[i];

		GLuint vertex_shader =
			wxrc_gl_compile_shader(GL_VERTEX_SHADER, job->vertex_src);
		if (vertex_shader == 0) {
			wlr_log(WLR_ERROR, "Failed to compile %s vertex shader",
				job->name);
			return false;
		}
		GLuint fragment_shader =
			wxrc_gl_compile_shader(GL_FRAGMENT_SHADER, job->fragment_src);
		if (fragment_shader == 0) {
			wlr_log(WLR_ERROR, "Failed to compile %s fragment shader",
				job->name);
			return false;
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
			wlr_log(WLR_ERROR, "Failed to link %s shader program: %s",
				job->name, log);
			return false;
		}

		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		*job->program_ptr = shader_program;
	}

	return true;
}

void wxrc_gl_finish(struct wxrc_gl *gl) {
	glDeleteProgram(gl->grid_program);
	glDeleteProgram(gl->texture_rgb_program);
	glDeleteProgram(gl->texture_external_program);
}

static const float fg_color[] = { 1.0, 1.0, 1.0, 1.0 };
static const float bg_color[] = { 0.08, 0.07, 0.16, 1.0 };

static void render_grid(struct wxrc_gl *gl, mat4 vp_matrix) {
	GLuint prog = gl->grid_program;

	GLint pos_loc = glGetAttribLocation(prog, "pos");
	GLint mvp_loc = glGetUniformLocation(prog, "mvp");
	GLint fg_color_loc = glGetUniformLocation(prog, "fg_color");

	glUseProgram(prog);

	glUniform4fv(fg_color_loc, 1, (GLfloat *)fg_color);

	mat4 model_matrix;
	glm_mat4_identity(model_matrix);
	glm_translate(model_matrix, (vec3){ 0.0, -1.0, 0.0 });
	glm_scale(model_matrix, (vec3){ 50.0, 50.0, 50.0 });
	glm_rotate(model_matrix, glm_rad(90.0), (vec3){ 1.0, 0.0, 0.0 });

	mat4 mvp_matrix = GLM_MAT4_IDENTITY_INIT;
	glm_mat4_mul(vp_matrix, model_matrix, mvp_matrix);

	glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (GLfloat *)mvp_matrix);

	size_t npoints = 4;
	const float points[] = {
		-0.5, -0.5, 0.0,
		-0.5, 0.5, 0.0,
		0.5, -0.5, 0.0,
		0.5, 0.5, 0.0,
	};

	GLint coords_per_point = sizeof(points) / sizeof(points[0]) / npoints;
	glVertexAttribPointer(pos_loc, coords_per_point,
		GL_FLOAT, GL_FALSE, 0, points);
	glEnableVertexAttribArray(pos_loc);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, npoints);

	glDisableVertexAttribArray(pos_loc);

	glUseProgram(0);
}

static void render_texture(struct wxrc_gl *gl, struct wlr_texture *tex,
		mat4 mvp_matrix) {
	if (!wlr_texture_is_gles2(tex)) {
		wlr_log(WLR_ERROR, "unsupported texture type");
		return;
	}

	struct wlr_gles2_texture_attribs attribs = {0};
	wlr_gles2_texture_get_attribs(tex, &attribs);

	GLuint prog;
	switch (attribs.target) {
	case GL_TEXTURE_2D:
		prog = gl->texture_rgb_program;
		break;
	case GL_TEXTURE_EXTERNAL_OES:
		prog = gl->texture_external_program;
		break;
	default:
		wlr_log(WLR_ERROR, "unsupported texture target %d", attribs.target);
		return;
	}

	GLint tex_coord_loc = glGetAttribLocation(prog, "tex_coord");
	GLint mvp_loc = glGetUniformLocation(prog, "mvp");
	GLint tex_loc = glGetUniformLocation(prog, "tex");
	GLint has_alpha_loc = glGetUniformLocation(prog, "has_alpha");
	GLint invert_y_loc = glGetUniformLocation(prog, "invert_y");

	glUseProgram(prog);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(attribs.target, attribs.tex);
	glTexParameteri(attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glUniform1i(tex_loc, 0);

	glUniform1i(invert_y_loc, !attribs.inverted_y);

	if (has_alpha_loc >= 0) {
		glUniform1i(has_alpha_loc, attribs.has_alpha);
	}

	glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (GLfloat *)mvp_matrix);

	size_t npoints = 4;
	const float points[] = {
		0.0, 0.0,
		0.0, 1.0,
		1.0, 0.0,
		1.0, 1.0,
	};

	GLint coords_per_point = sizeof(points) / sizeof(points[0]) / npoints;
	glVertexAttribPointer(tex_coord_loc, coords_per_point,
		GL_FLOAT, GL_FALSE, 0, points);
	glEnableVertexAttribArray(tex_coord_loc);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, npoints);

	glDisableVertexAttribArray(tex_coord_loc);

	glUseProgram(0);
}

struct render_data {
	struct wxrc_gl *gl;
	struct wxrc_view *view;
	mat4 vp_matrix;
};

static void render_surface_iterator(struct wlr_surface *surface,
		int sx, int sy, void *_data) {
	struct render_data *data = _data;

	if (surface->buffer == NULL || surface->buffer->texture == NULL) {
		return;
	}

	mat4 model_matrix;
	wxrc_view_get_2d_model_matrix(data->view, surface, sx, sy, model_matrix);

	mat4 mvp_matrix;
	glm_mat4_mul(data->vp_matrix, model_matrix, mvp_matrix);

	render_texture(data->gl, surface->buffer->texture, mvp_matrix);
}

static void render_2d_view(struct wxrc_gl *gl,
		mat4 vp_matrix, struct wxrc_view *view) {
	struct render_data data = {
		.gl = gl,
		.view = view,
	};
	glm_mat4_copy(vp_matrix, data.vp_matrix);

	wxrc_view_for_each_surface(view, render_surface_iterator, &data);
}

static void render_xr_shell_view(struct wxrc_gl *gl, mat4 vp_matrix,
		struct wxrc_xr_view *xr_view, struct wxrc_view *view) {
	struct wlr_surface *surface = view->surface;

	struct wlr_buffer *buffer = surface->buffer;
	if (buffer == NULL) {
		return;
	}

	/* TODO: Test for other kinds of buffers */
	struct wxrc_zxr_composite_buffer_v1 *comp_buffer =
		wxrc_zxr_composite_buffer_v1_from_buffer(buffer);
	struct wlr_texture *tex = wxrc_zxr_composite_buffer_v1_for_view(
		comp_buffer, xr_view->wl_view,
		ZXR_COMPOSITE_BUFFER_V1_BUFFER_TYPE_PIXEL_BUFFER);
	if (tex == NULL) {
		/* TODO: Don't show on one view if we can't show on all views */
		wlr_log(WLR_DEBUG, "Attempted to render XR surface without texture");
		return;
	}

	mat4 mvp_matrix = GLM_MAT4_IDENTITY_INIT;
	glm_translate(mvp_matrix, (vec3){ -1.0, -1.0, 0.0 });
	glm_scale(mvp_matrix, (vec3){ 2.0, 2.0, 1.0 });
	render_texture(gl, tex, mvp_matrix);
}

static void render_view(struct wxrc_gl *gl, mat4 vp_matrix,
		struct wxrc_xr_view *xr_view, struct wxrc_view *view) {
	if (wxrc_view_is_xr_shell(view)) {
		render_xr_shell_view(gl, vp_matrix, xr_view, view);
	} else {
		render_2d_view(gl, vp_matrix, view);
	}
}

static void render_cursor(struct wxrc_server *server,
		struct wxrc_gl *gl, mat4 vp_matrix, mat4 cursor_matrix) {
	int hotspot_x, hotspot_y, scale;
	struct wlr_texture *tex = wxrc_cursor_get_texture(&server->cursor,
		&hotspot_x, &hotspot_y, &scale);
	if (tex == NULL) {
		return;
	}

	int width, height;
	wlr_texture_get_size(tex, &width, &height);

	float scale_x = (float)width / WXRC_SURFACE_SCALE / scale;
	float scale_y = (float)height / WXRC_SURFACE_SCALE / scale;

	mat4 model_matrix;
	glm_mat4_copy(cursor_matrix, model_matrix);
	glm_scale(model_matrix, (vec3){ scale_x, scale_y, 1.0 });

	/* Re-origin the cursor to the center and apply hotspot */
	glm_translate(model_matrix, (vec3){
		-(float)hotspot_x / width,
		-1.0 + (float)hotspot_y / height, 0 });

	mat4 mvp_matrix = GLM_MAT4_IDENTITY_INIT;
	glm_mat4_mul(vp_matrix, model_matrix, mvp_matrix);

	render_texture(gl, tex, mvp_matrix);
}

void wxrc_gl_render_view(struct wxrc_server *server, struct wxrc_xr_view *view,
		mat4 view_matrix, mat4 projection_matrix) {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	mat4 vp_matrix = GLM_MAT4_IDENTITY_INIT;
	glm_mat4_mul(projection_matrix, view_matrix, vp_matrix);

	render_grid(&server->gl, vp_matrix);

	// Disable writing to the depth buffer, so that we never render views
	// intersected but still correctly integrate them in the 3D scene
	glDepthMask(GL_FALSE);

	struct wxrc_view *wxrc_view;
	wl_list_for_each_reverse(wxrc_view, &server->views, link) {
		if (!wxrc_view->mapped) {
			continue;
		}
		render_view(&server->gl, vp_matrix, view, wxrc_view);
	}

	if (server->seat->pointer_state.focused_surface != NULL) {
		render_cursor(server, &server->gl, vp_matrix, server->cursor.matrix);
	}

	glDepthMask(GL_TRUE);
}

void wxrc_gl_render_xr_view(struct wxrc_server *server, struct wxrc_xr_view *view,
		XrView *xr_view, GLuint framebuffer, GLuint image, GLuint depth_buffer) {
	uint32_t width = view->config.recommendedImageRectWidth;
	uint32_t height = view->config.recommendedImageRectHeight;

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glViewport(0, 0, width, height);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		image, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
		depth_buffer, 0);

	mat4 view_matrix;
	wxrc_xr_view_get_matrix(xr_view, view_matrix);
	glm_mat4_inv(view_matrix, view_matrix);

	mat4 projection_matrix;
	wxrc_get_projection_matrix(xr_view, projection_matrix);

	wxrc_gl_render_view(server, view, view_matrix, projection_matrix);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void wxrc_get_projection_matrix(XrView *xr_view, mat4 projection_matrix) {
	wxrc_xr_projection_from_fov(&xr_view->fov, 0.05, 100.0, projection_matrix);
}
