#include <cglm/cglm.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include "render.h"
#include "xr.h"

static const GLchar vertex_shader_src[] =
	"#version 100\n"
	"\n"
	"attribute vec3 pos;\n"
	"uniform mat4 mvp;\n"
	"\n"
	"void main() {\n"
	"	gl_Position = mvp * vec4(pos, 1.0);\n"
	"}\n";

static const GLchar fragment_shader_src[] =
	"#version 100\n"
	"precision mediump float;\n"
	"\n"
	"void main() {\n"
	"	if (fract(gl_FragCoord.x / 100.0) < 0.01 ||\n"
	"			fract(gl_FragCoord.y / 100.0) < 0.01) {\n"
	"		gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
	"	} else {\n"
	"		gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
	"	}\n"
	"}\n";

static GLuint wxrc_gl_compile_shader(GLuint type, const GLchar *src) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		const char *name;
		switch (type) {
		case GL_VERTEX_SHADER:
			name = "vertex";
			break;
		case GL_FRAGMENT_SHADER:
			name = "fragment";
			break;
		default:
			abort();
		}

		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		wlr_log(WLR_ERROR, "Failed to compile %s shader: %s\n", name, log);
		return 0;
	}

	return shader;
}

bool wxrc_gl_init(struct wxrc_gl *gl) {
	GLuint vertex_shader =
		wxrc_gl_compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
	if (vertex_shader == 0) {
		return false;
	}
	GLuint fragment_shader =
		wxrc_gl_compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
	if (fragment_shader == 0) {
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
		wlr_log(WLR_ERROR, "Failed to link shader program: %s\n", log);
		return false;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	gl->shader_program = shader_program;

	return true;
}

void wxrc_gl_finish(struct wxrc_gl *gl) {
	glDeleteProgram(gl->shader_program);
}

void wxrc_gl_render_view(struct wxrc_gl *gl, struct wxrc_xr_view *view,
		GLuint framebuffer, GLuint image) {
	uint32_t width = view->config.recommendedImageRectWidth;
	uint32_t height = view->config.recommendedImageRectHeight;

	GLint pos_loc = glGetAttribLocation(gl->shader_program, "pos");
	GLint mvp_loc = glGetUniformLocation(gl->shader_program, "mvp");

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glViewport(0, 0, width, height);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		image, 0);

	glClearColor(0.0, 0.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(gl->shader_program);

	mat4 model_matrix;
	glm_rotate_make(model_matrix, -45.0, (vec3){ 1.0, 0.0, 0.0 });

	mat4 view_matrix;
	glm_translate_make(view_matrix, (vec3){ 0.0, 0.0, -3.0 });

	mat4 projection_matrix;
	glm_perspective_default((float)width / height, projection_matrix);

	mat4 mvp_matrix = GLM_MAT4_IDENTITY_INIT;
	glm_mat4_mul(projection_matrix, view_matrix, mvp_matrix);
	glm_mat4_mul(mvp_matrix, model_matrix, mvp_matrix);

	glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (GLfloat *)mvp_matrix);

	const float points[] = {
		-0.5, -0.5, 0.0,
		-0.5, 0.5, 0.0,
		0.5, -0.5, 0.0,
		0.5, 0.5, 0.0,
	};

	glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, 0, points);
	glEnableVertexAttribArray(pos_loc);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(pos_loc);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
