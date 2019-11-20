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

static bool bind_model(struct wxrc_gltf_model *model) {
	cgltf_scene *scene = model->data->scene;
	for (size_t i = 0; i < scene->nodes_count; i++) {
		bind_node(scene->nodes[i]);
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

static void render_primitive(cgltf_primitive *primitive) {
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
		mode = GL_TRIANGLE_STRIP; // TODO: what happened here?
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
		wlr_log(WLR_ERROR, "unsupported material");
		return;
	}

	GLint program;
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	GLint pos_loc = glGetAttribLocation(program, "pos");
	GLint normal_loc = glGetAttribLocation(program, "normal");
	GLint base_color_loc = glGetUniformLocation(program, "base_color");

	glUniform4fv(base_color_loc, 1,
		primitive->material->pbr_metallic_roughness.base_color_factor);

	if (!enable_accessor(pos_attr->data, pos_loc) ||
			!enable_accessor(normal_attr->data, normal_loc)) {
		return;
	}

	glDrawArrays(mode, 0, pos_attr->data->count);

	glDisableVertexAttribArray(pos_loc);
	glDisableVertexAttribArray(normal_loc);
}

static void render_mesh(cgltf_mesh *mesh) {
	for (size_t i = 0; i < mesh->primitives_count; i++) {
		render_primitive(&mesh->primitives[i]);
	}
}

static void render_node(cgltf_node *node) {
	if (node->mesh != NULL) {
		render_mesh(node->mesh);
	}
	for (size_t i = 0; i < node->children_count; i++) {
		render_node(node->children[i]);
	}
}

void wxrc_gltf_model_render(struct wxrc_gltf_model *model, mat4 mvp_matrix) {
	GLuint prog = model->gl->gltf_program;

	GLint mvp_loc = glGetUniformLocation(prog, "mvp");

	glUseProgram(prog);

	glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (GLfloat *)mvp_matrix);

	cgltf_scene *scene = model->data->scene;
	for (size_t i = 0; i < scene->nodes_count; i++) {
		render_node(scene->nodes[i]);
	}

	glUseProgram(0);
}
