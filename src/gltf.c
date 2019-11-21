#define _POSIX_C_SOURCE 200809L
#include <png.h>
#include <stdio.h>
#include <wlr/util/log.h>
#include "gltf.h"
#include "render.h"

static bool bind_mesh(cgltf_mesh *mesh) {
	printf("mesh: %s\n", mesh->name);
	for (size_t i = 0; i < mesh->primitives_count; i++) {
		cgltf_primitive *primitive = &mesh->primitives[i];
		for (size_t j = 0; j < primitive->attributes_count; j++) {
			cgltf_attribute *attrib = &primitive->attributes[j];
			printf("attribute: %s\n", attrib->name);
			// TODO
		}
	}
	return true;
}

static bool bind_node(cgltf_node *node) {
	printf("node: %s\n", node->name);
	if (node->mesh != NULL) {
		bind_mesh(node->mesh);
	}
	for (size_t i = 0; i < node->children_count; i++) {
		bind_node(node->children[i]);
	}
	return true;
}

static GLuint bind_texture(cgltf_texture *texture) {
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

	GLuint tex;
	glGenTextures(1, &tex);

	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	free(data);

	return tex;
}

static bool bind_model(struct wxrc_gltf_model *model) {
	cgltf_scene *scene = model->data->scene;
	for (size_t i = 0; i < scene->nodes_count; i++) {
		bind_node(scene->nodes[i]);
	}

	model->textures = calloc(model->data->textures_count, sizeof(GLuint));
	for (size_t i = 0; i < model->data->textures_count; i++) {
		model->textures[i] = bind_texture(&model->data->textures[i]);
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

	if (!bind_model(model)) {
		return false;
	}

	return true;
}

void wxrc_gltf_model_finish(struct wxrc_gltf_model *model) {
	glDeleteTextures(model->data->textures_count, model->textures);
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

static bool enable_accessor(cgltf_accessor *accessor, GLint loc) {
	cgltf_buffer_view *buffer_view = accessor->buffer_view;

	GLenum array_type;
	switch (accessor->component_type) {
	case cgltf_component_type_r_8:
		array_type = GL_BYTE;
		break;
	case cgltf_component_type_r_8u:
		array_type = GL_UNSIGNED_BYTE;
		break;
	case cgltf_component_type_r_16:
		array_type = GL_SHORT;
		break;
	case cgltf_component_type_r_16u:
		array_type = GL_UNSIGNED_SHORT;
		break;
	case cgltf_component_type_r_32u:
		array_type = GL_UNSIGNED_INT;
		break;
	case cgltf_component_type_r_32f:
		array_type = GL_FLOAT;
		break;
	default:
		wlr_log(WLR_ERROR, "invalid buffer view type");
		return false;
	}

	if (accessor->is_sparse) {
		wlr_log(WLR_ERROR, "sparse accessors not yet implemented");
		return false;
	}

	size_t num_components = cgltf_num_components(accessor->type);
	size_t offset = buffer_view->offset + accessor->offset;
	void *data = (uint8_t *)buffer_view->buffer->data + offset;

	glVertexAttribPointer(loc, num_components, array_type,
		accessor->normalized, buffer_view->stride, data);
	glEnableVertexAttribArray(loc);

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

	if (!enable_accessor(pos_attr->data, pos_loc) ||
			!enable_accessor(normal_attr->data, normal_loc)) {
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
		if (!enable_accessor(texcoord_attr->data, texcoord_loc)) {
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

	if (primitive->indices != NULL) {
		// TODO
		wlr_log(WLR_ERROR, "primitives with indices not yet supported");
		return;
	} else {
		glDrawArrays(mode, 0, pos_attr->data->count);
	}

	glDisableVertexAttribArray(pos_loc);
	glDisableVertexAttribArray(normal_loc);
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
