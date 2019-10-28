#ifndef _WXRC_OXR_UTIL_H
#define _WXRC_OXR_UTIL_H
#include <openxr/openxr.h>

/** Gets a user-friendly description of this XrResult */
const char *wxrc_xr_result_str(XrResult r);

/** Gets a user-friendly description of this XrStructureType */
const char *wxrc_xr_structure_type_str(XrStructureType t);

/** Logs an XR result with wlr_log */
void wxrc_log_xr_result(const char *entrypoint, XrResult r);

#endif
