#include <stdlib.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <unistd.h>
#include "input.h"
#include "server.h"
#include "view.h"

void focus_view(struct wxrc_view *view, struct wlr_surface *surface) {
	if (view == NULL) {
		return;
	}
	if (surface == NULL) {
		surface = view->surface;
	}

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
		focus_view(next_view, NULL);
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

void handle_new_input(struct wl_listener *listener, void *data) {
	struct wxrc_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	wlr_log(WLR_DEBUG, "New input device '%s'", device->name);
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		handle_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		/* TODO */
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
	server->cursor_pos[0] = 0.0;
	server->cursor_pos[1] = 0.0;
	server->cursor_pos[2] = -2.0;

	server->new_input.notify = handle_new_input;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);
}