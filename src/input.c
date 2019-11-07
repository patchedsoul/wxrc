#include <stdlib.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include "input.h"
#include "server.h"
#include "view.h"

void focus_view(struct wxrc_view *view, struct wlr_surface *surface) {
	if (view == NULL) {
		return;
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
		wlr_seat_keyboard_notify_enter(seat, xdg_view->xdg_surface->surface,
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

static void keyboard_handle_key(
		struct wl_listener *listener, void *data) {
	struct wxrc_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct wxrc_server *server = keyboard->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *seat = server->seat;

	/* TODO: keybindings */
	wlr_seat_set_keyboard(seat, keyboard->device);
	wlr_seat_keyboard_notify_key(seat, event->time_msec,
		event->keycode, event->state);
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

	server->new_input.notify = handle_new_input;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);
}
