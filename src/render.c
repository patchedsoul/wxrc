#include <cglm/cglm.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/render/gles2.h>
#include "render.h"
#include "server.h"
#include "backend.h"
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
	"uniform vec4 bg_color;\n"
	"\n"
	"varying vec3 vertex_pos;\n"
	"\n"
	"void main() {\n"
	"	if (fract(vertex_pos.x * 100.0) < 0.01 ||\n"
	"			fract(vertex_pos.y * 100.0) < 0.01) {\n"
	"		float a = distance(vertex_pos, vec3(0)) * 7.0;\n"
	"		a = min(a, 1.0);\n"
	"		gl_FragColor = mix(fg_color, bg_color, a);\n"
	"	} else {\n"
	"		gl_FragColor = bg_color;\n"
	"	}\n"
	"}\n";

static const GLchar texture_vertex_shader_src[] =
	"#version 100\n"
	"\n"
	"attribute vec2 tex_coord;\n"
	"uniform mat4 mvp;\n"
	"\n"
	"varying vec2 vertex_tex_coord;\n"
	"\n"
	"void main() {\n"
	"	vertex_tex_coord = tex_coord;\n"
	"	gl_Position = mvp * vec4(tex_coord, 0.0, 1.0);\n"
	"}\n";

static const GLchar texture_fragment_shader_src[] =
	"#version 100\n"
	"precision mediump float;\n"
	"\n"
	"uniform sampler2D tex;\n"
	"\n"
	"varying vec2 vertex_tex_coord;\n"
	"\n"
	"void main() {\n"
	"	gl_FragColor = texture2D(tex, vertex_tex_coord);\n"
	"	gl_FragColor.a = 1.0;\n"
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
		wlr_log(WLR_ERROR, "Failed to compile shader: %s\n", log);
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
			.name = "texture",
			.vertex_src = texture_vertex_shader_src,
			.fragment_src = texture_fragment_shader_src,
			.program_ptr = &gl->texture_program,
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
			wlr_log(WLR_ERROR, "Failed to link %s shader program: %s\n",
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
	glDeleteProgram(gl->texture_program);
}

static void wxrc_xr_vector3f_to_cglm(const XrVector3f *in, vec3 out) {
	out[0] = in->x;
	out[1] = in->y;
	out[2] = in->z;
}

static void wxrc_xr_quaternion_to_cglm(const XrQuaternionf *in, versor out) {
	out[0] = in->x;
	out[1] = in->y;
	out[2] = in->z;
	out[3] = in->w;
}

static const float fg_color[] = { 1.0, 1.0, 1.0, 1.0 };
static const float bg_color[] = { 0.08, 0.07, 0.16, 1.0 };

static void render_grid(struct wxrc_gl *gl, mat4 vp_matrix) {
	GLint pos_loc = glGetAttribLocation(gl->grid_program, "pos");
	GLint mvp_loc = glGetUniformLocation(gl->grid_program, "mvp");
	GLint fg_color_loc = glGetUniformLocation(gl->grid_program, "fg_color");
	GLint bg_color_loc = glGetUniformLocation(gl->grid_program, "bg_color");

	glUseProgram(gl->grid_program);

	glUniform4fv(fg_color_loc, 1, (GLfloat *)fg_color);
	glUniform4fv(bg_color_loc, 1, (GLfloat *)bg_color);

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

static void render_surface(struct wxrc_gl *gl, mat4 vp_matrix,
		struct wlr_surface *surface) {
	struct wlr_buffer *buffer = surface->buffer;
	if (buffer == NULL) {
		return;
	}
	struct wlr_texture *tex = buffer->texture;
	if (tex == NULL) {
		return;
	}
	if (!wlr_texture_is_gles2(tex)) {
		return;
	}

	struct wlr_gles2_texture_attribs attribs = {0};
	wlr_gles2_texture_get_attribs(tex, &attribs);
	/* TODO: add support for external textures, alpha channel, inverted_y */
	if (attribs.target != GL_TEXTURE_2D || attribs.inverted_y) {
		return;
	}

	int width, height;
	wlr_texture_get_size(tex, &width, &height);

	GLint tex_coord_loc = glGetAttribLocation(gl->texture_program, "tex_coord");
	GLint mvp_loc = glGetUniformLocation(gl->texture_program, "mvp");
	GLint tex_loc = glGetUniformLocation(gl->texture_program, "tex");

	glUseProgram(gl->texture_program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(attribs.target, attribs.tex);
	glTexParameteri(attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glUniform1i(tex_loc, 0);

	float scale_x = -width / 300.0;
	float scale_y = -height / 300.0;

	mat4 model_matrix;
	glm_mat4_identity(model_matrix);
	glm_scale(model_matrix, (vec3){ scale_x, scale_y, 1.0 });
	glm_translate(model_matrix, (vec3){ 0.0, 0.0, 2.0 });

	mat4 mvp_matrix = GLM_MAT4_IDENTITY_INIT;
	glm_mat4_mul(vp_matrix, model_matrix, mvp_matrix);

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

void wxrc_gl_render_view(struct wxrc_server *server, struct wxrc_xr_view *view,
		XrView *xr_view, GLuint framebuffer, GLuint image) {
	uint32_t width = view->config.recommendedImageRectWidth;
	uint32_t height = view->config.recommendedImageRectHeight;

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glViewport(0, 0, width, height);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		image, 0);

	glClearColor(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
	glClear(GL_COLOR_BUFFER_BIT);

	mat4 view_matrix;
	versor orientation;
	vec3 position;
	wxrc_xr_quaternion_to_cglm(&xr_view->pose.orientation, orientation);
	glm_quat_mat4(orientation, view_matrix);
	wxrc_xr_vector3f_to_cglm(&xr_view->pose.position, position);
	/* TODO: don't zero out Y-axis */
	position[1] = 0;
	glm_translate(view_matrix, position);
	glm_mat4_inv(view_matrix, view_matrix);

	mat4 projection_matrix;
	wxrc_xr_projection_from_fov(&xr_view->fov, 0.05, 100.0, projection_matrix);

	mat4 vp_matrix = GLM_MAT4_IDENTITY_INIT;
	glm_mat4_mul(projection_matrix, view_matrix, vp_matrix);

	render_grid(&server->gl, vp_matrix);

	if (server->current_surface != NULL) {
		render_surface(&server->gl, vp_matrix, server->current_surface);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
