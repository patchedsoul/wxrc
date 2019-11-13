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

void focus_view(struct wxrc_view *view) {
	if (view == NULL) {
		return;
	}
	struct wlr_surface *surface = view->surface;

	struct wxrc_server *server = view->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		return;
	}
	if (prev_surface) {
		struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(
					seat->keyboard_state.focused_surface);
		if (previous) {
			wlr_xdg_toplevel_set_activated(previous, false);
		}
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	wl_list_remove(&view->link);
	wl_list_insert(&server->views, &view->link);
	switch (view->view_type) {
	case WXRC_VIEW_XDG_SHELL:;
		struct wxrc_xdg_shell_view *xdg_view =
			(struct wxrc_xdg_shell_view *)view;
		wlr_xdg_toplevel_set_activated(xdg_view->xdg_surface, true);
		wlr_seat_keyboard_notify_enter(seat, surface,
				keyboard->keycodes, keyboard->num_keycodes,
				&keyboard->modifiers);
		break;
	default:
		break;
	}
}

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
		focus_view(next_view);
		wl_list_remove(&current_view->link);
		wl_list_insert(server->views.prev, &current_view->link);
		break;
	case XKB_KEY_Return:
		spawn_terminal();
		break;
	default:
		return false;
	}
	return true;
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
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
	if ((modifiers & WLR_MODIFIER_ALT) && event->state == WLR_KEY_PRESSED) {
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

void wxrc_update_pointer(struct wxrc_server *server, XrView *xr_view,
		uint32_t time) {
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
	float focus_dist = FLT_MAX;
	vec3 cursor_pos;
	vec3 cursor_rot;
	float focus_sx, focus_sy;
	struct wxrc_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped) {
			continue;
		}

		mat4 model_matrix;
		wxrc_view_get_2d_model_matrix(view, model_matrix);

		vec3 intersection;
		float sx, sy;
		if (!wxrc_intersect_surface_line(view->surface, model_matrix, view->position,
				view->rotation, position, dir, intersection, &sx, &sy)) {
			continue;
		}

		if (!wlr_surface_point_accepts_input(view->surface, sx, sy)) {
			continue;
		}

		float dist = glm_vec3_distance(position, intersection);
		// Add an epsilon to dist to avoid Z-index rounding errors fighting
		if (dist + 0.01 < focus_dist) {
			focus = view->surface;
			focus_sx = sx;
			focus_sy = sy;
			focus_dist = dist;
			glm_vec3_copy(intersection, cursor_pos);
			glm_vec3_copy(view->rotation, cursor_rot);
		}
	}

	server->pointer_has_focus = focus != NULL;
	if (focus != NULL) {
		wlr_seat_pointer_notify_enter(server->seat, focus, focus_sx, focus_sy);
		wlr_seat_pointer_notify_motion(server->seat, time, focus_sx, focus_sy);
		wlr_seat_pointer_notify_frame(server->seat);

		glm_mat4_identity(server->cursor_matrix);
		glm_translate(server->cursor_matrix, cursor_pos);
		wxrc_mat4_rotate(server->cursor_matrix, cursor_rot);
	} else {
		wlr_seat_pointer_clear_focus(server->seat);
	}
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
	struct wlr_event_pointer_button *event = data;

	wlr_seat_pointer_notify_button(pointer->server->seat,
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

void handle_new_input(struct wl_listener *listener, void *data) {
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

void wxrc_input_init(struct wxrc_server *server) {
	wl_list_init(&server->keyboards);
	wl_list_init(&server->pointers);

	server->seat = wlr_seat_create(server->wl_display, "seat0");
	server->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(server->cursor_mgr, 2);

	struct wlr_renderer *renderer = wlr_backend_get_renderer(server->backend);
	struct wlr_xcursor *xcursor =
		wlr_xcursor_manager_get_xcursor(server->cursor_mgr, "left_ptr", 2);
	server->xcursor_image = xcursor->images[0];
	server->cursor = wlr_texture_from_pixels(renderer,
			WL_SHM_FORMAT_ARGB8888, server->xcursor_image->width * 4,
			server->xcursor_image->width, server->xcursor_image->height,
			server->xcursor_image->buffer);

	server->new_input.notify = handle_new_input;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);
}
