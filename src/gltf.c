#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <png.h>
#include <stdio.h>
#include <wlr/util/log.h>
#include "gltf.h"
#include "render.h"

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

static GLuint upload_texture(cgltf_texture *texture) {
	cgltf_image *image = texture->image;
	cgltf_buffer_view *buffer_view = image->buffer_view;
	cgltf_buffer *buffer = buffer_view->buffer;

	uint8_t *buf_data = (uint8_t *)buffer->data + buffer_view->offset;

	if (png_sig_cmp(buf_data, 0, buffer_view->size)) {
		wlr_log(WLR_ERROR, "Malformed PNG file");
		return 0;
	}

	FILE *f = fmemopen(buf_data, buffer_view->size, "r");

	png_structp png =
		png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info = png_create_info_struct(png);
	png_init_io(png, f);

	png_read_info(png, info);
	int width = png_get_image_width(png, info);
	int height = png_get_image_height(png, info);
	int color_type = png_get_color_type(png, info);
	int bit_depth = png_get_bit_depth(png, info);

	if (bit_depth == 16) {
		png_set_strip_16(png);
	}
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png);
	}
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		png_set_expand_gray_1_2_4_to_8(png);
	}
	if (png_get_valid(png, info, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png);
	}
	if (color_type == PNG_COLOR_TYPE_RGB ||
			color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
	}
	if (color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb(png);
	}

	int row_bytes = width * 4;
	uint8_t *data = malloc(row_bytes * height);
	png_bytep *row_pointers = malloc(height * sizeof(png_bytep));
	for (int i = 0; i < height; i++) {
		row_pointers[i] = data + row_bytes * i;
	}
	png_read_image(png, row_pointers);
	free(row_pointers);

	png_destroy_read_struct(&png, &info, NULL);

	fclose(f);

	wlr_log(WLR_DEBUG, "Uploading texture: %s (%dx%d, %dB)", texture->name,
		width, height, row_bytes * height);

	GLuint tex = 0;
	glGenTextures(1, &tex);

	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	free(data);

	return tex;
}

static bool upload_model(struct wxrc_gltf_model *model) {
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
		model->textures[i] = upload_texture(&model->data->textures[i]);
		if (model->textures[i] == 0) {
			return false;
		}
	}

	return true;
}

bool wxrc_gltf_model_init(struct wxrc_gltf_model *model, struct wxrc_gl *gl,
		const char *path) {
	model->gl = gl;

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

static void render_mesh(struct wxrc_gltf_model *model, cgltf_mesh *mesh) {
	for (size_t i = 0; i < mesh->primitives_count; i++) {
		render_primitive(model, &mesh->primitives[i]);
	}
}

static void render_node(struct wxrc_gltf_model *model, cgltf_node *node) {
	if (node->mesh != NULL) {
		render_mesh(model, node->mesh);
	}
	for (size_t i = 0; i < node->children_count; i++) {
		render_node(model, node->children[i]);
	}
}

void wxrc_gltf_model_render(struct wxrc_gltf_model *model, mat4 mvp_matrix) {
	GLuint prog = model->gl->gltf_program;

	GLint mvp_loc = glGetUniformLocation(prog, "mvp");

	glUseProgram(prog);

	glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (GLfloat *)mvp_matrix);

	cgltf_scene *scene = model->data->scene;
	for (size_t i = 0; i < scene->nodes_count; i++) {
		render_node(model, scene->nodes[i]);
	}

	glUseProgram(0);
}
