#ifndef _WXRC_OXR_UTIL_H
#define _WXRC_OXR_UTIL_H

#include <cglm/cglm.h>
#include <openxr/openxr.h>

/** Gets a user-friendly description of this XrResult */
const char *wxrc_xr_result_str(XrResult r);

/** Gets a user-friendly description of this XrStructureType */
const char *wxrc_xr_structure_type_str(XrStructureType t);

/** Logs an XR result with wlr_log */
void wxrc_log_xr_result(const char *entrypoint, XrResult r);

void wxrc_xr_projection_from_fov(const XrFovf *fov, float near_z, float far_z,
	mat4 dest);

void wxrc_xr_vector3f_to_cglm(const XrVector3f *in, vec3 out);
void wxrc_xr_quaternion_to_cglm(const XrQuaternionf *in, versor out);

#endif
