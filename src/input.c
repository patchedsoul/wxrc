#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <unistd.h>
#include "input.h"
#include "mathutil.h"
#include "server.h"
#include "view.h"
#include "xrutil.h"

static void keyboard_handle_modifiers(
		struct wl_listener *listener, void *data) {
	struct wxrc_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->device);
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->device->keyboard->modifiers);
}

static void spawn_terminal(void) {
	pid_t pid = fork();
	if (pid < 0) {
		wlr_log_errno(WLR_ERROR, "fork failed");
	} else if (pid == 0) {
		const char *term = getenv("TERMINAL");
		if (!term) {
			term = "alacritty";
		}
		execl("/bin/sh", "/bin/sh", "-c", term, (void *)NULL);
		wlr_log_errno(WLR_ERROR, "execl failed");
		exit(1);
	}
}

static bool handle_keybinding(struct wxrc_server *server, xkb_keysym_t sym) {
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_Tab:
		if (wl_list_length(&server->views) < 2) {
			break;
		}
		struct wxrc_view *current_view = wl_container_of(
			server->views.next, current_view, link);
		struct wxrc_view *next_view = wl_container_of(
			current_view->link.next, next_view, link);
		wxrc_set_focus(next_view);
		wl_list_remove(&current_view->link);
		wl_list_insert(server->views.prev, &current_view->link);
		break;
	case XKB_KEY_Return:
		spawn_terminal();
		break;
	case XKB_KEY_q:;
		struct wxrc_view *view = wxrc_get_focus(server);
		if (view != NULL) {
			wxrc_view_close(view);
		}
		break;
	default:
		return false;
	}
	return true;
}

static bool keyboard_meta_pressed(struct wxrc_keyboard *keyboard) {
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
	return modifiers & WLR_MODIFIER_ALT;
}

static void keyboard_handle_key(
		struct wl_listener *listener, void *data) {
	struct wxrc_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct wxrc_server *server = keyboard->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *seat = server->seat;

	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->device->keyboard->xkb_state, keycode, &syms);

	/* TODO: In the future we'll likely want a more sophisticated approach */
	bool handled = false;
	if (keyboard_meta_pressed(keyboard) && event->state == WLR_KEY_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, syms[i]);
		}
	}

	if (!handled) {
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

void handle_new_keyboard(struct wxrc_server *server,
		struct wlr_input_device *device) {
	struct wxrc_keyboard *keyboard =
		calloc(1, sizeof(struct wxrc_keyboard));
	keyboard->server = server;
	keyboard->device = device;

	/* TODO: Source keymap et al from parent Wayland compositor if possible */
	struct xkb_rule_names rules = { 0 };
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(device->keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&device->keyboard->events.key, &keyboard->key);

	wlr_seat_set_keyboard(server->seat, device);

	wl_list_insert(&server->keyboards, &keyboard->link);
}

static struct wxrc_view *view_at(struct wxrc_server *server, XrView *xr_view,
		mat4 cursor_matrix, struct wlr_surface **surface_ptr,
		float *sx_ptr, float *sy_ptr) {
	versor orientation;
	vec3 position;
	wxrc_xr_quaternion_to_cglm(&xr_view->pose.orientation, orientation);
	wxrc_xr_vector3f_to_cglm(&xr_view->pose.position, position);
	/* TODO: don't zero out Y-axis */
	position[1] = 0;

	vec3 dir = { 0.0, 0.0, -1.0 };
	mat4 pointer_rot_matrix;
	glm_quat_mat4(orientation, pointer_rot_matrix);
	wxrc_mat4_rotate(pointer_rot_matrix, server->pointer_rotation);
	glm_vec3_rotate_m4(pointer_rot_matrix, dir, dir);

	struct wlr_surface *focus = NULL;
	struct wxrc_view *focus_view = NULL;
	float focus_dist = FLT_MAX;
	vec3 cursor_pos;
	vec3 cursor_rot;
	float focus_sx, focus_sy;
	struct wxrc_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped || wxrc_view_is_xr_shell(view)) {
			continue;
		}

		mat4 model_matrix;
		wxrc_view_get_2d_model_matrix(view, NULL, 0, 0, model_matrix);

		vec3 intersection;
		float sx, sy;
		if (!wxrc_intersect_surface_plane_line(view->surface, model_matrix,
				view->position, view->rotation, position, dir, intersection,
				&sx, &sy)) {
			continue;
		}

		double child_sx, child_sy;
		struct wlr_surface *surface = wxrc_view_surface_at(view, sx, sy,
			&child_sx, &child_sy);
		if (surface == NULL) {
			continue;
		}

		float dist = glm_vec3_distance(position, intersection);
		// Add an epsilon to dist to avoid Z-index rounding errors fighting
		if (dist + 0.01 < focus_dist) {
			focus = surface;
			focus_view = view;
			focus_sx = child_sx;
			focus_sy = child_sy;
			focus_dist = dist;
			glm_vec3_copy(intersection, cursor_pos);
			glm_vec3_copy(view->rotation, cursor_rot);
		}
	}

	if (focus == NULL) {
		return NULL;
	}

	if (surface_ptr != NULL) {
		*surface_ptr = focus;
	}
	if (sx_ptr != NULL || sy_ptr != NULL) {
		assert(sx_ptr != NULL && sy_ptr != NULL);
		*sx_ptr = focus_sx;
		*sy_ptr = focus_sy;
	}

	glm_mat4_identity(cursor_matrix);
	glm_translate(cursor_matrix, cursor_pos);
	wxrc_mat4_rotate(cursor_matrix, cursor_rot);

	return focus_view;
}

static void update_pointer_default(struct wxrc_server *server, XrView *xr_view,
		uint32_t time) {
	float sx, sy;
	struct wlr_surface *surface;
	struct wxrc_view *focus = view_at(server, xr_view, server->cursor.matrix,
		&surface, &sx, &sy);
	if (focus != NULL) {
		wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);
		wlr_seat_pointer_notify_frame(server->seat);
	} else {
		wlr_seat_pointer_clear_focus(server->seat);
	}
}

static void update_pointer_move(struct wxrc_server *server, XrView *xr_view) {
	wlr_seat_pointer_clear_focus(server->seat);

	struct wxrc_view *view = wxrc_get_focus(server);
	if (view == NULL) {
		return;
	}

	mat4 view_matrix;
	wxrc_xr_view_get_matrix(xr_view, view_matrix);
	wxrc_mat4_rotate(view_matrix, server->pointer_rotation);

	vec3 pos = { 0.0, 0.0, -glm_vec3_norm(view->position) };
	glm_vec3_rotate_m4(view_matrix, pos, pos);

	vec3 rot;
	glm_euler_angles(view_matrix, rot);

	glm_vec3_copy(pos, view->position);
	glm_vec3_copy(rot, view->rotation);
}

void wxrc_update_pointer(struct wxrc_server *server, XrView *xr_view,
		uint32_t time) {
	switch (server->seatop) {
	case WXRC_SEATOP_DEFAULT:
		update_pointer_default(server, xr_view, time);
		return;
	case WXRC_SEATOP_MOVE:
		update_pointer_move(server, xr_view);
		return;
	}
	abort();
}

static void clamp(float *f, float min, float max) {
	if (*f < min) {
		*f = min;
	}
	if (*f > max) {
		*f = max;
	}
}

static void pointer_handle_motion(struct wl_listener *listener, void *data) {
	struct wxrc_pointer *pointer = wl_container_of(listener, pointer, motion);
	struct wxrc_server *server = pointer->server;
	struct wlr_event_pointer_motion *event = data;

	server->pointer_rotation[1] += -event->delta_x * 0.001;
	server->pointer_rotation[0] += -event->delta_y * 0.001;

	XrFovf *fov = &server->xr_views[0].fov;
	float angle_padding = glm_rad(20);
	clamp(&server->pointer_rotation[0],
		fmin(fov->angleLeft, fov->angleRight) + angle_padding,
		fmax(fov->angleLeft, fov->angleRight) - angle_padding);
	clamp(&server->pointer_rotation[1],
		fmin(fov->angleUp, fov->angleDown) + angle_padding,
		fmax(fov->angleUp, fov->angleDown) - angle_padding);
}

static void pointer_handle_button(struct wl_listener *listener, void *data) {
	struct wxrc_pointer *pointer = wl_container_of(listener, pointer, button);
	struct wxrc_server *server = pointer->server;
	struct wlr_event_pointer_button *event = data;

	switch (event->state) {
	case WLR_BUTTON_PRESSED:;
		mat4 cursor_matrix;
		struct wxrc_view *view = view_at(server, &server->xr_views[0],
			cursor_matrix, NULL, NULL, NULL);
		if (view == NULL) {
			break;
		}
		wxrc_set_focus(view);

		bool meta_pressed = false;
		struct wxrc_keyboard *keyboard;
		wl_list_for_each(keyboard, &server->keyboards, link) {
			if (keyboard_meta_pressed(keyboard)) {
				meta_pressed = true;
				break;
			}
		}

		if (meta_pressed) {
			server->seatop = WXRC_SEATOP_MOVE;
			return;
		}
		break;
	case WLR_BUTTON_RELEASED:
		if (server->seatop != WXRC_SEATOP_DEFAULT) {
			server->seatop = WXRC_SEATOP_DEFAULT;
			return;
		}
		break;
	}

	wlr_seat_pointer_notify_button(server->seat,
		event->time_msec, event->button, event->state);
}

static void pointer_handle_axis(struct wl_listener *listener, void *data) {
	struct wxrc_pointer *pointer = wl_container_of(listener, pointer, axis);
	struct wlr_event_pointer_axis *event = data;

	wlr_seat_pointer_notify_axis(pointer->server->seat,
		event->time_msec, event->orientation, event->delta,
		event->delta_discrete, event->source);
}

static void pointer_handle_frame(struct wl_listener *listener, void *data) {
	struct wxrc_pointer *pointer = wl_container_of(listener, pointer, frame);

	wlr_seat_pointer_notify_frame(pointer->server->seat);
}

static void handle_new_pointer(struct wxrc_server *server,
		struct wlr_input_device *device) {
	struct wxrc_pointer *pointer = calloc(1, sizeof(*pointer));
	pointer->server = server;
	pointer->device = device;

	wl_list_insert(&server->pointers, &pointer->link);

	pointer->motion.notify = pointer_handle_motion;
	wl_signal_add(&device->pointer->events.motion, &pointer->motion);
	pointer->button.notify = pointer_handle_button;
	wl_signal_add(&device->pointer->events.button, &pointer->button);
	pointer->axis.notify = pointer_handle_axis;
	wl_signal_add(&device->pointer->events.axis, &pointer->axis);
	pointer->frame.notify = pointer_handle_frame;
	wl_signal_add(&device->pointer->events.frame, &pointer->frame);
}

static void handle_new_input(struct wl_listener *listener, void *data) {
	struct wxrc_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	wlr_log(WLR_DEBUG, "New input device '%s'", device->name);
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		handle_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		handle_new_pointer(server, device);
		break;
	default:
		break;
	}

	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

static void cursor_reset(struct wxrc_cursor *cursor) {
	wlr_texture_destroy(cursor->xcursor_texture);
	cursor->xcursor_texture = NULL;
	cursor->xcursor_image = NULL;

	wl_list_remove(&cursor->surface_destroy.link);
	wl_list_init(&cursor->surface_destroy.link);
	cursor->surface = NULL;
}

void wxrc_cursor_set_xcursor(struct wxrc_cursor *cursor,
		struct wlr_xcursor *xcursor) {
	cursor_reset(cursor);

	cursor->xcursor_image = xcursor->images[0];

	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(cursor->server->backend);
	cursor->xcursor_texture = wlr_texture_from_pixels(renderer,
		WL_SHM_FORMAT_ARGB8888, cursor->xcursor_image->width * 4,
		cursor->xcursor_image->width, cursor->xcursor_image->height,
		cursor->xcursor_image->buffer);
}

static void cursor_handle_surface_destroy(struct wl_listener *listener,
		void *data) {
	struct wxrc_cursor *cursor =
		wl_container_of(listener, cursor, surface_destroy);
	cursor_reset(cursor);
}

void wxrc_cursor_set_surface(struct wxrc_cursor *cursor,
		struct wlr_surface *surface, int hotspot_x, int hotspot_y) {
	cursor_reset(cursor);

	cursor->surface = surface;
	cursor->hotspot_x = hotspot_x;
	cursor->hotspot_y = hotspot_y;

	cursor->surface_destroy.notify = cursor_handle_surface_destroy;
	wl_signal_add(&surface->events.destroy, &cursor->surface_destroy);
}

struct wlr_texture *wxrc_cursor_get_texture(struct wxrc_cursor *cursor,
		int *hotspot_x, int *hotspot_y, int *scale) {
	*hotspot_x = *hotspot_y = *scale = 0;

	if (cursor->surface != NULL && cursor->surface->buffer != NULL &&
			cursor->surface->buffer->texture != NULL) {
		*hotspot_x = cursor->hotspot_x + cursor->surface->sx;
		*hotspot_y = cursor->hotspot_y + cursor->surface->sy;
		*scale = cursor->surface->current.scale;
		return cursor->surface->buffer->texture;
	}

	if (cursor->xcursor_texture != NULL) {
		*hotspot_x = cursor->xcursor_image->hotspot_x;
		*hotspot_y = cursor->xcursor_image->hotspot_y;
		*scale = 2;
		return cursor->xcursor_texture;
	}

	return NULL;
}

static void handle_request_set_cursor(struct wl_listener *listener,
		void *data) {
	struct wxrc_server *server =
		wl_container_of(listener, server, request_set_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;

	struct wl_client *focused_client = NULL;
	struct wlr_surface *focused_surface =
		server->seat->pointer_state.focused_surface;
	if (focused_surface != NULL) {
		focused_client = wl_resource_get_client(focused_surface->resource);
	}

	// TODO: check cursor mode
	if (focused_client == NULL ||
			event->seat_client->client != focused_client) {
		wlr_log(WLR_DEBUG, "Denying request to set cursor from unfocused client");
		return;
	}

	wxrc_cursor_set_surface(&server->cursor, event->surface,
		event->hotspot_x, event->hotspot_y);
}

void wxrc_input_init(struct wxrc_server *server) {
	wl_list_init(&server->keyboards);
	wl_list_init(&server->pointers);

	server->seat = wlr_seat_create(server->wl_display, "seat0");
	server->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(server->cursor_mgr, 2);

	server->cursor.server = server;
	wl_list_init(&server->cursor.surface_destroy.link);

	server->new_input.notify = handle_new_input;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);

	server->request_set_cursor.notify = handle_request_set_cursor;
	wl_signal_add(&server->seat->events.request_set_cursor,
		&server->request_set_cursor);

	struct wlr_xcursor *xcursor =
		wlr_xcursor_manager_get_xcursor(server->cursor_mgr, "left_ptr", 2);
	wxrc_cursor_set_xcursor(&server->cursor, xcursor);
}
