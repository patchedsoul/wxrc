#include <wlr/backend/interface.h>

struct wxrc_xr_backend {
	struct wlr_backend base;

	struct wl_display *remote_display;
	struct wlr_egl egl;
	struct wlr_renderer *renderer;

	struct wl_listener local_display_destroy;
};

bool wxrc_backend_is_xr(struct wlr_backend *wlr_backend);
