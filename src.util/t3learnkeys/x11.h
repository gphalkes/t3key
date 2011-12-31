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
#ifndef LEARNKEYS_X11_H
#define LEARNKEYS_X11_H

#ifdef USE_XLIB
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput.h>
#else
#include <xcb/xcb.h>
/* Defines copied from X11/keysymdefs.h */
#define XK_BackSpace                     0xff08  /* Back space, back char */
#define XK_Tab                           0xff09
#define XK_Linefeed                      0xff0a  /* Linefeed, LF */
#define XK_Delete                        0xffff  /* Delete, rubout */
#define XK_Insert                        0xff63  /* Insert, insert here */
#define XK_KP_Home                       0xff95
#define XK_KP_Left                       0xff96
#define XK_KP_Up                         0xff97
#define XK_KP_Right                      0xff98
#define XK_KP_Down                       0xff99
#define XK_KP_Page_Up                    0xff9a
#define XK_KP_Page_Down                  0xff9b
#define XK_KP_End                        0xff9c
#define XK_KP_Begin                      0xff9d
#define XK_KP_Insert                     0xff9e
#define XK_KP_Delete                     0xff9f
#define XK_KP_Multiply                   0xffaa
#define XK_KP_Add                        0xffab
#define XK_KP_Subtract                   0xffad
#define XK_KP_Divide                     0xffaf
#define XK_KP_Enter                      0xff8d  /* Enter */
#define XK_Home                          0xff50
#define XK_Left                          0xff51  /* Move left, left arrow */
#define XK_Up                            0xff52  /* Move up, up arrow */
#define XK_Right                         0xff53  /* Move right, right arrow */
#define XK_Down                          0xff54  /* Move down, down arrow */
#define XK_Page_Up                       0xff55
#define XK_Page_Down                     0xff56
#define XK_End                           0xff57  /* EOL */
#define XK_a                             0x0061
#define XK_F1                            0xffbe

#define ControlMask XCB_KEY_BUT_MASK_CONTROL
#define ShiftMask XCB_KEY_BUT_MASK_SHIFT
#define Mod1Mask XCB_KEY_BUT_MASK_MOD_1

typedef xcb_connection_t Display;
typedef xcb_window_t Window;
typedef xcb_keysym_t KeySym;
typedef xcb_keycode_t KeyCode;
typedef xcb_generic_event_t XEvent;
typedef xcb_key_press_event_t XKeyEvent;
typedef enum { False, True } Bool;

#define NoSymbol XCB_NO_SYMBOL
#define KeyPressMask XCB_EVENT_MASK_KEY_PRESS
#define KeyReleaseMask XCB_EVENT_MASK_KEY_RELEASE
#define KeyPress XCB_KEY_PRESS
#define KeyRelease XCB_KEY_RELEASE
#define PointerRoot XCB_INPUT_FOCUS_POINTER_ROOT
#define FollowKeyboard XCB_INPUT_FOCUS_FOLLOW_KEYBOARD

#define DefaultRootWindow(display) (xcb_setup_roots_iterator(xcb_get_setup(display)).data->root)

Display *XOpenDisplay(const char *name);
void XCloseDisplay(Display *display);
KeySym XKeycodeToKeysym(Display *display, KeyCode keycode, int index);
void XGetInputFocus(Display *display, Window *focus, int *revert_to_return);
int XChangeKeyboardMapping(Display *display, int first_keycode, int keysyms_per_keycode, KeySym *keysyms, int num_codes);
void XSendEvent(Display *display, Window w, Bool propagate, long event_mask, XEvent *event_send);
void XSync(Display *display, Bool discard);
void XQueryPointer(Display *display, Window w, Window *root_return, Window *child_return,
		int *root_x_return, int *root_y_return, int *win_x_return, int *win_y_return, unsigned int *mask_return);
#endif


#define X_KEY_SYM(_x) _x
extern Display *display;
extern Window focus_window;

Bool initX11(void);
void send_event(KeySym keysym, unsigned state);
#endif
