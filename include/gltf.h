#ifndef WXRC_GLTF_H
#define WXRC_GLTF_H

#include <cglm/cglm.h>
#include <cgltf.h>
#include <GLES2/gl2.h>
#include <stdbool.h>

struct wxrc_gltf_model {
	struct wxrc_gl *gl;
	char *path;
	cgltf_data *data;
	GLuint *vbos; // one per buffer view
	GLuint *textures;
};

bool wxrc_gltf_model_init(struct wxrc_gltf_model *model, struct wxrc_gl *gl,
	const char *path);
void wxrc_gltf_model_finish(struct wxrc_gltf_model *model);
void wxrc_gltf_model_render(struct wxrc_gltf_model *model, mat4 mvp_matrix);

#endif
