/* Copyright (C) 2011 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdlib.h>
#include <stdio.h>

#ifndef NO_AUTOLEARN
#include "x11.h"

void fatal(const char *fmt, ...);

Display *display;
Window focus_window;
extern int reprogram_code;
extern int option_auto_learn;

#ifdef USE_XLIB
static void fill_key_event(Display *display, Window focus_window, XKeyEvent *event, int type, KeyCode keycode, unsigned state) {
	event->type = type;
	event->serial = 0;
	event->send_event = 0;
	event->display = display;
	event->window = focus_window;
	event->subwindow = None;
	event->root = DefaultRootWindow(display);
	event->time = CurrentTime;
	event->x = 1;
	event->y = 1;
	event->x_root = 1;
	event->y_root = 1;
	event->state = state;
	event->keycode = keycode;
	event->same_screen = True;
}
#else
Display *XOpenDisplay(const char *name) {
	return xcb_connect(name, NULL);
}

void XCloseDisplay(Display *display) {
	xcb_disconnect(display);
}

KeySym XKeycodeToKeysym(Display *display, KeyCode keycode, int index) {
	xcb_get_keyboard_mapping_cookie_t cookie;
	xcb_get_keyboard_mapping_reply_t *reply;
	xcb_keysym_t result = NoSymbol;

	cookie = xcb_get_keyboard_mapping(display, keycode, 1);
	reply = xcb_get_keyboard_mapping_reply(display, cookie, NULL);

	if (reply == NULL)
		fatal("Error in X11 communication\n");

	result = xcb_get_keyboard_mapping_keysyms(reply)[index];
	free(reply);
	return result;
}

void XGetInputFocus(Display *display, Window *focus, int *revert_to_return) {
	xcb_get_input_focus_cookie_t cookie;
	xcb_get_input_focus_reply_t *reply;

	cookie = xcb_get_input_focus(display);
 	reply = xcb_get_input_focus_reply(display, cookie, NULL);

	if (reply == NULL)
		fatal("Error in X11 communication\n");

	*focus = reply->focus;
	*revert_to_return = reply->revert_to;
	free(reply);
}

int XChangeKeyboardMapping(Display *display, int first_keycode, int keysyms_per_keycode,
		KeySym *keysyms, int num_codes)
{
	xcb_void_cookie_t cookie = xcb_change_keyboard_mapping_checked(display, num_codes, first_keycode, keysyms_per_keycode, keysyms);
	xcb_generic_error_t *error = xcb_request_check(display, cookie);
	if (error != NULL)
		fatal("Error in X11 communication\n");
	return 0;
}

void XSendEvent(Display *display, Window w, Bool propagate, long event_mask, XEvent *event_send) {
	xcb_void_cookie_t cookie = xcb_send_event_checked (display, propagate, w, event_mask, (const char *) event_send);
	xcb_generic_error_t *error = xcb_request_check(display, cookie);
	if (error != NULL)
		fatal("Error in X11 communication\n");
}

void XSync(Display *display, Bool discard) {
	xcb_generic_event_t *event;

	if (!discard)
		fatal("XSync called for unimplemented functionality\n");
	xcb_flush(display);
	while ((event = xcb_poll_for_event(display)) != NULL)
		free(event);
}

void XQueryPointer(Display *display, Window w, Window *root_return, Window *child_return,
		int *root_x_return, int *root_y_return, int *win_x_return, int *win_y_return, unsigned int *mask_return)
{
	xcb_query_pointer_cookie_t cookie = xcb_query_pointer(display, w);
	xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(display, cookie, NULL);

	if (reply == NULL)
		fatal("Error in X11 communication\n");

	*root_return = reply->root;
	*child_return = reply->child;
	*root_x_return = reply->root_x;
	*root_y_return = reply->root_y;
	*win_x_return = reply->win_x;
	*win_y_return = reply->win_y;
	*mask_return = reply->mask;
	free(reply);
}

static void fill_key_event(Display *display, Window focus_window, XKeyEvent *event, int type, KeyCode keycode, unsigned state) {
	event->response_type = type;
	event->sequence = 0;
	event->event = focus_window;
	event->child = XCB_WINDOW_NONE;
	event->root = DefaultRootWindow(display);
	event->time = XCB_TIME_CURRENT_TIME;
	event->event_x = 1;
	event->event_y = 1;
	event->root_x = 1;
	event->root_y = 1;
	event->state = state;
	event->detail = keycode;
	event->same_screen = 1;

}
#endif

Bool initX11(void) {
	int revert_to_return;

	if ((display = XOpenDisplay(NULL)) == NULL)
		return False;

	/* Find a key code to reprogram with all the different possible keys. This is
	   required because not all keys that we want to test are available on all keyboards
	   (e.g. the function keys up to F36). If we find a key that is not used, we
	   can temporarily reprogram that key to the key we need for each key stroke. */
	for (reprogram_code = 255; reprogram_code >= 0; reprogram_code--)
		if (XKeycodeToKeysym(display, reprogram_code, 0) == NoSymbol)
			break;

	if (reprogram_code == -1)
		fatal("Can not use auto-learn because no reprogrammable keys were found\n");

	XGetInputFocus(display, &focus_window, &revert_to_return);

	if (focus_window == PointerRoot) {
		Window root, child;
		int root_x, root_y, win_x, win_y;
		unsigned int mask;
		XQueryPointer(display, DefaultRootWindow(display), &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);
		focus_window = child;
	}

	if (focus_window == PointerRoot || focus_window == FollowKeyboard) {
		option_auto_learn = False;
		fatal("Auto-learn disabled because the current focus window could not be determined\n");
		XCloseDisplay(display);
	}
	return True;
}

void send_event(KeySym keysym, unsigned state) {
	XKeyEvent event;

	/* If an unused key is available, reprogram that for sending the key stroke to
	   the terminal. This will allow us to use keys which are not available on the
	   user's keyboard. */
	if (XChangeKeyboardMapping(display, reprogram_code, 1, &keysym, 1) != 0)
		fatal("Could not reprogram key\n");

	fill_key_event(display, focus_window, &event, KeyPress, reprogram_code, state);

	XSendEvent(display, focus_window, True, KeyPressMask | KeyReleaseMask, (XEvent *) &event);

	fill_key_event(display, focus_window, &event, KeyRelease, reprogram_code, state);

	XSendEvent(display, focus_window, True, KeyPressMask | KeyReleaseMask, (XEvent *) &event);
	XSync(display, True);
}
#endif /* -NO_AUTOLEARN */
