#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <wlr/util/log.h>
#include "gltf.h"
#include "img.h"
#include "render.h"

static bool infer_buffer_view_type(cgltf_buffer_view *buffer_view,
		cgltf_buffer_view_type type) {
	if (buffer_view->type == cgltf_buffer_view_type_invalid) {
		buffer_view->type = type;
	} else if (buffer_view->type != type) {
		wlr_log(WLR_ERROR, "Invalid buffer view type: got %d, want %d",
			buffer_view->type, type);
		return false;
	}
	return true;
}

static bool infer_primitive_buffer_views_type(cgltf_primitive *primitive) {
	if (primitive->indices != NULL && !infer_buffer_view_type(
			primitive->indices->buffer_view, cgltf_buffer_view_type_indices)) {
		return false;
	}

	for (size_t i = 0; i < primitive->attributes_count; i++) {
		cgltf_attribute *attr = &primitive->attributes[i];

		cgltf_buffer_view *buffer_view = attr->data->buffer_view;
		if (!infer_buffer_view_type(buffer_view, cgltf_buffer_view_type_vertices)) {
			return false;
		}
	}

	return true;
}

static GLenum buffer_view_type_target(cgltf_buffer_view_type t) {
	switch (t) {
	case cgltf_buffer_view_type_invalid:
		break;
	case cgltf_buffer_view_type_indices:
		return GL_ELEMENT_ARRAY_BUFFER;
	case cgltf_buffer_view_type_vertices:
		return GL_ARRAY_BUFFER;
	}
	wlr_log(WLR_ERROR, "Invalid buffer view type");
	return GL_NONE;
}

static GLuint upload_buffer_view(cgltf_buffer_view *buffer_view) {
	cgltf_buffer *buffer = buffer_view->buffer;
	if (buffer->data == NULL) {
		wlr_log(WLR_ERROR, "buffer data unavailable");
		return 0;
	}

	GLenum target = buffer_view_type_target(buffer_view->type);
	if (target == GL_NONE) {
		return 0;
	}

	assert(buffer_view->offset + buffer_view->size <= buffer->size);
	void *data = (uint8_t *)buffer->data + buffer_view->offset;

	GLuint vbo = 0;
	glGenBuffers(1, &vbo);
	glBindBuffer(target, vbo);
	glBufferData(target, buffer_view->size, data, GL_STATIC_DRAW);
	glBindBuffer(target, 0);

	return vbo;
}

static GLuint upload_texture(struct wxrc_gltf_model *model,
		cgltf_texture *texture) {
	cgltf_image *image = texture->image;

	FILE *f;
	cgltf_buffer_view *buffer_view = image->buffer_view;
	if (buffer_view != NULL) {
		cgltf_buffer *buffer = buffer_view->buffer;
		uint8_t *buf_data = (uint8_t *)buffer->data + buffer_view->offset;
		f = fmemopen(buf_data, buffer_view->size, "r");
	} else if (image->uri != NULL) {
		char *model_path = strdup(model->path);
		char *dir = dirname(model_path);
		char image_path[PATH_MAX];
		snprintf(image_path, sizeof(image_path), "%s/%s", dir, image->uri);
		free(model_path);

		f = fopen(image_path, "r");
	}

	if (f == NULL) {
		wlr_log(WLR_ERROR, "Failed to open image");
		return 0;
	}

	int width, height;
	bool has_alpha;
	void *data =
		wxrc_load_image(f, image->mime_type, &width, &height, &has_alpha);
	if (data == NULL) {
		return 0;
	}

	fclose(f);

	wlr_log(WLR_DEBUG, "Uploading texture: %s (%dx%d)", texture->name,
		width, height);

	GLuint tex = 0;
	glGenTextures(1, &tex);

	GLenum format = has_alpha ? GL_RGBA : GL_RGB;
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
		GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	free(data);

	return tex;
}

static bool upload_model(struct wxrc_gltf_model *model) {
	for (size_t i = 0; i < model->data->meshes_count; i++) {
		cgltf_mesh *mesh = &model->data->meshes[i];
		for (size_t j = 0; j < mesh->primitives_count; j++) {
			if (!infer_primitive_buffer_views_type(&mesh->primitives[j])) {
				return false;
			}
		}
	}

	model->vbos = calloc(model->data->buffer_views_count, sizeof(GLuint));
	for (size_t i = 0; i < model->data->buffer_views_count; i++) {
		if (model->data->buffer_views[i].type ==
				cgltf_buffer_view_type_invalid) {
			continue;
		}
		model->vbos[i] = upload_buffer_view(&model->data->buffer_views[i]);
		if (model->vbos[i] == 0) {
			return false;
		}
	}

	model->textures = calloc(model->data->textures_count, sizeof(GLuint));
	for (size_t i = 0; i < model->data->textures_count; i++) {
		model->textures[i] = upload_texture(model, &model->data->textures[i]);
		if (model->textures[i] == 0) {
			return false;
		}
	}

	return true;
}

bool wxrc_gltf_model_init(struct wxrc_gltf_model *model, struct wxrc_gl *gl,
		const char *path) {
	model->gl = gl;
	model->path = strdup(path);

	cgltf_options options = {0};
	cgltf_result result =
		cgltf_parse_file(&options, path, &model->data);
	if (result != cgltf_result_success) {
		wlr_log(WLR_ERROR, "Failed to load glTF model");
		return false;
	}

	if (cgltf_load_buffers(&options, model->data, path) != cgltf_result_success) {
		wlr_log(WLR_ERROR, "Failed to load glTF buffers");
		return false;
	}

	if (!upload_model(model)) {
		return false;
	}

	return true;
}

void wxrc_gltf_model_finish(struct wxrc_gltf_model *model) {
	glDeleteTextures(model->data->textures_count, model->textures);
	glDeleteBuffers(model->data->buffer_views_count, model->vbos);
	free(model->textures);
	cgltf_free(model->data);
	free(model->path);
}

static cgltf_attribute *get_primitive_attribute(cgltf_primitive *primitive,
		const char *name) {
	for (size_t i = 0; i < primitive->attributes_count; i++) {
		cgltf_attribute *attrib = &primitive->attributes[i];
		if (strcmp(attrib->name, name) == 0) {
			return attrib;
		}
	}
	return NULL;
}

static GLenum component_type(cgltf_component_type t) {
	switch (t) {
	case cgltf_component_type_invalid:
		break;
	case cgltf_component_type_r_8:
		return GL_BYTE;
	case cgltf_component_type_r_8u:
		return GL_UNSIGNED_BYTE;
	case cgltf_component_type_r_16:
		return GL_SHORT;
	case cgltf_component_type_r_16u:
		return GL_UNSIGNED_SHORT;
	case cgltf_component_type_r_32u:
		return GL_UNSIGNED_INT;
	case cgltf_component_type_r_32f:
		return GL_FLOAT;
	}
	wlr_log(WLR_ERROR, "invalid component type");
	return GL_NONE;
}

static void bind_buffer_view(struct wxrc_gltf_model *model,
		cgltf_buffer_view *buffer_view) {
	size_t buffer_view_idx = buffer_view - model->data->buffer_views;
	GLuint vbo = model->vbos[buffer_view_idx];
	assert(vbo != 0);
	glBindBuffer(buffer_view_type_target(buffer_view->type), vbo);
}

static void unbind_buffer_view(cgltf_buffer_view *buffer_view) {
	glBindBuffer(buffer_view_type_target(buffer_view->type), 0);
}

static bool enable_vertex_accessor(struct wxrc_gltf_model *model,
		cgltf_accessor *accessor, GLint loc) {
	cgltf_buffer_view *buffer_view = accessor->buffer_view;
	assert(buffer_view->type == cgltf_buffer_view_type_vertices);

	GLenum array_type = component_type(accessor->component_type);
	if (array_type == GL_NONE) {
		return false;
	}

	if (accessor->is_sparse) {
		wlr_log(WLR_ERROR, "sparse accessors not yet implemented");
		return false;
	}

	bind_buffer_view(model, buffer_view);

	size_t num_components = cgltf_num_components(accessor->type);
	GLintptr offset = accessor->offset;
	glEnableVertexAttribArray(loc);
	glVertexAttribPointer(loc, num_components, array_type,
		accessor->normalized, buffer_view->stride, (GLvoid *)offset);

	unbind_buffer_view(buffer_view);

	return true;
}

static void render_primitive(struct wxrc_gltf_model *model,
		cgltf_primitive *primitive) {
	cgltf_attribute *pos_attr = get_primitive_attribute(primitive, "POSITION");
	if (pos_attr == NULL) {
		wlr_log(WLR_ERROR, "primitive missing POSITION attribute");
		return;
	}
	if (pos_attr->type != cgltf_attribute_type_position) {
		wlr_log(WLR_ERROR, "POSITION attribute has invalid type");
		return;
	}

	cgltf_attribute *normal_attr = get_primitive_attribute(primitive, "NORMAL");
	if (normal_attr == NULL) {
		wlr_log(WLR_ERROR, "primitive missing NORMAL attribute");
		return;
	}
	if (normal_attr->type != cgltf_attribute_type_normal) {
		wlr_log(WLR_ERROR, "NORMAL attribute has invalid type");
		return;
	}

	GLenum mode;
	switch (primitive->type) {
	case cgltf_primitive_type_points:
		mode = GL_POINTS;
		break;
	case cgltf_primitive_type_lines:
		mode = GL_LINES;
		break;
	case cgltf_primitive_type_line_loop:
		mode = GL_LINE_LOOP;
		break;
	case cgltf_primitive_type_line_strip:
		mode = GL_LINE_STRIP;
		break;
	case cgltf_primitive_type_triangles:
		mode = GL_TRIANGLES;
		break;
	case cgltf_primitive_type_triangle_strip:
		mode = GL_TRIANGLE_STRIP;
		break;
	case cgltf_primitive_type_triangle_fan:
		mode = GL_TRIANGLE_FAN;
		break;
	default:
		wlr_log(WLR_ERROR, "invalid primitive type");
		return;
	}

	cgltf_material *material = primitive->material;
	if (!material->has_pbr_metallic_roughness) {
		wlr_log(WLR_ERROR, "unsupported material"); // TODO
		return;
	}
	cgltf_pbr_metallic_roughness *pbr_metallic_roughness =
		&material->pbr_metallic_roughness;

	GLint program = model->gl->gltf_program;
	GLint pos_loc = glGetAttribLocation(program, "pos");
	GLint normal_loc = glGetAttribLocation(program, "normal");
	GLint texcoord_loc = glGetAttribLocation(program, "texcoord");
	GLint use_tex_loc = glGetUniformLocation(program, "use_tex");
	GLint base_color_loc = glGetUniformLocation(program, "base_color");
	GLint tex_loc = glGetUniformLocation(program, "tex");

	if (!enable_vertex_accessor(model, pos_attr->data, pos_loc) ||
			!enable_vertex_accessor(model, normal_attr->data, normal_loc)) {
		return;
	}

	glUniform4fv(base_color_loc, 1,
		pbr_metallic_roughness->base_color_factor);

	bool use_tex = pbr_metallic_roughness->base_color_texture.texture != NULL;
	glUniform1i(use_tex_loc, use_tex);
	if (use_tex) {
		cgltf_attribute *texcoord_attr =
			get_primitive_attribute(primitive, "TEXCOORD_0");
		if (texcoord_attr == NULL) {
			wlr_log(WLR_ERROR, "TEXCOORD_0 attribute missing");
			return;
		}
		if (texcoord_attr->type != cgltf_attribute_type_texcoord) {
			wlr_log(WLR_ERROR, "TEXCOORD_0 attribute has invalid type");
			return;
		}
		if (!enable_vertex_accessor(model, texcoord_attr->data, texcoord_loc)) {
			return;
		}
		size_t tex_idx = pbr_metallic_roughness->base_color_texture.texture -
			model->data->textures;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, model->textures[tex_idx]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glUniform1i(tex_loc, 0);
	}

	cgltf_accessor *indices = primitive->indices;
	if (indices != NULL) {
		assert(indices->buffer_view->type == cgltf_buffer_view_type_indices);
		bind_buffer_view(model, indices->buffer_view);

		GLenum type = component_type(indices->component_type);
		GLintptr offset = indices->offset;
		glDrawElements(mode, indices->count, type, (GLvoid *)offset);

		unbind_buffer_view(indices->buffer_view);
	} else {
		glDrawArrays(mode, 0, pos_attr->data->count);
	}

	glDisableVertexAttribArray(pos_loc);
	glDisableVertexAttribArray(normal_loc);
	if (use_tex) {
		glDisableVertexAttribArray(texcoord_loc);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
}

static void render_mesh(struct wxrc_gltf_model *model, cgltf_mesh *mesh,
		mat4 matrix) {
	GLuint prog = model->gl->gltf_program;
	GLint mvp_loc = glGetUniformLocation(prog, "mvp");
	glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (GLfloat *)matrix);

	for (size_t i = 0; i < mesh->primitives_count; i++) {
		render_primitive(model, &mesh->primitives[i]);
	}
}

static void render_node(struct wxrc_gltf_model *model, cgltf_node *node,
		mat4 matrix) {
	mat4 node_matrix;
	glm_mat4_copy(matrix, node_matrix);
	if (node->has_matrix) {
		mat4 m; // needs to be aligned
		assert(sizeof(m) == sizeof(node->matrix));
		memcpy(m, node->matrix, sizeof(m));
		glm_mat4_mul(node_matrix, m, node_matrix);
	} else {
		if (node->has_translation) {
			glm_translate(node_matrix, node->translation);
		}
		if (node->has_rotation) {
			versor q; // needs to be aligned
			assert(sizeof(q) == sizeof(node->rotation));
			memcpy(q, node->rotation, sizeof(q));

			mat4 rot;
			glm_quat_mat4(q, rot);
			glm_mat4_mul(node_matrix, rot, node_matrix);
		}
		if (node->has_scale) {
			glm_scale(node_matrix, node->scale);
		}
	}

	if (node->mesh != NULL) {
		render_mesh(model, node->mesh, node_matrix);
	}
	for (size_t i = 0; i < node->children_count; i++) {
		render_node(model, node->children[i], node_matrix);
	}
}

void wxrc_gltf_model_render(struct wxrc_gltf_model *model, mat4 matrix) {
	GLuint prog = model->gl->gltf_program;

	glUseProgram(prog);

	cgltf_scene *scene = model->data->scene;
	for (size_t i = 0; i < scene->nodes_count; i++) {
		render_node(model, scene->nodes[i], matrix);
	}

	glUseProgram(0);
}
