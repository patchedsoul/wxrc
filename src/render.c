#include <stdlib.h>
#include <wlr/util/log.h>
#include "render.h"
#include "xr.h"

static const GLchar vertex_shader_src[] =
	"void main() {\n"
	"}\n";

static const GLchar fragment_shader_src[] =
	"void main() {\n"
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
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glViewport(0, 0, view->config.recommendedImageRectWidth,
		view->config.recommendedImageRectHeight);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		image, 0);

	glClearColor(0.0, 0.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
