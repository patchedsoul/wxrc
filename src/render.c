#include <stdlib.h>
#include <wlr/util/log.h>
#include "render.h"
#include "xr.h"

static const GLchar vertex_shader_src[] =
	"#version 100\n"
	"\n"
	"attribute vec3 in_pos;\n"
	"\n"
	"void main() {\n"
	"	gl_Position = vec4(in_pos, 1.0);\n"
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

	GLint in_pos = glGetAttribLocation(gl->shader_program, "in_pos");

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glViewport(0, 0, width, height);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		image, 0);

	glClearColor(0.0, 0.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(gl->shader_program);

	const float points[] = {
		-0.5, -0.5, 0.0,
		-0.5, 0.5, 0.0,
		0.5, -0.5, 0.0,
		0.5, 0.5, 0.0,
	};

	glVertexAttribPointer(in_pos, 3, GL_FLOAT, GL_FALSE, 0, points);
	glEnableVertexAttribArray(in_pos);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(in_pos);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
