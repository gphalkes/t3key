#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <curses.h>
#include <term.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>


#define MAX_SEQUENCE 50
#define ESC 27
#define KEY_TIMEOUT 10

//~ #define TESTING

//FIXME: test modifier-letter as well
//FIXME: check calloc/malloc return values!

typedef struct {
	const char *name;
	const char *identifier;
	long keysym;
} name_mapping_t;

static name_mapping_t keynames[] = {
#ifndef TESTING
	{ "Insert", "insert", XK_Insert },
	{ "Home", "home", XK_Home },
	{ "Page Up", "page_up", XK_Page_Up },
	{ "Delete", "delete", XK_Delete },
	{ "End", "end", XK_End },
	{ "Page Down", "page_down", XK_Page_Down },
#endif
	{ "Up", "up", XK_Up },
#ifndef TESTING
	{ "Left", "left", XK_Left },
	{ "Down", "down", XK_Down },
	{ "Right", "right", XK_Right },
	{ "Keypad /", "kp_div", XK_KP_Divide },
	{ "Keypad *", "kp_mul", XK_KP_Multiply },
	{ "Keypad -", "kp_minus", XK_KP_Subtract },
	{ "Keypad Home", "kp_home", XK_KP_Home },
	{ "Keypad Up", "kp_up", XK_KP_Up },
	{ "Keypad Page Up", "kp_page_up", XK_KP_Page_Up },
	{ "Keypad +", "kp_plus", XK_KP_Add },
	{ "Keypad Left", "kp_left", XK_KP_Left },
	{ "Keypad Center", "kp_center", XK_KP_Begin },
	{ "Keypad Right", "kp_right", XK_KP_Right },
	{ "Keypad End", "kp_end", XK_KP_End },
	{ "Keypad Down", "kp_down", XK_KP_Down },
	{ "Keypad Page Down", "kp_page_down", XK_KP_Page_Down },
	{ "Keypad Insert", "kp_insert", XK_KP_Insert },
	{ "Keypad Delete", "kp_delete", XK_KP_Delete },
	{ "Keypad Enter", "kp_enter", XK_KP_Enter },
	{ "Tab", "tab", XK_Tab },
	{ "Backspace", "backspace", XK_BackSpace }
#endif
};

static name_mapping_t *functionkeys;

static name_mapping_t modifiers[] = {
	{ "", "", 0 },
#ifndef TESTING
	{ "Control ", "+c", ControlMask },
	{ "Meta ", "+m", Mod1Mask },
	{ "Shift ", "+s", ShiftMask },
	{ "Control+Meta ", "+cm", ControlMask | Mod1Mask },
	{ "Control+Shift ", "+cs", ControlMask | ShiftMask },
	{ "Meta+Shift ", "+ms", Mod1Mask | ShiftMask },
	{ "Control+Meta+Shift ", "+cms", ControlMask | Mod1Mask | ShiftMask },
#endif
};

#define SIZEOF(_x) ((sizeof(_x)/sizeof(_x[0])))

typedef struct sequence_t {
	struct sequence_t *next;
	char seq[MAX_SEQUENCE * 4 + 1];
	name_mapping_t *modifiers;
	name_mapping_t *keynames;
	struct sequence_t *duplicate;
	bool remove;
} sequence_t;

typedef struct map_list_t map_list_t;

typedef struct map_t {
	struct map_t *next;
	char *esc_seq_enter;
	char *esc_seq_enter_name;
	char *esc_seq_leave;
	char *esc_seq_leave_name;
	char *name;
	sequence_t *sequences;
	map_list_t *collected_from;
} map_t;

struct map_list_t {
	struct map_list_t *next;
	map_t *map;
};

static struct termios saved;
static fd_set inset;
static bool xterm_restore;
static int maxfkeys = -1;
static bool x11_auto;
static Display *display;
static Window root, focus_window;


/** Alert the user of a fatal error and quit.
    @param fmt The format string for the message. See fprintf(3) for details.
    @param ... The arguments for printing.
*/
static void fatal(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static void restore_terminal(void) {
	char *rmkx = tigetstr("rmkx");
	if (rmkx != NULL && rmkx != (void *) -1)
		printf("%s", rmkx);
	if (xterm_restore)
		printf("\x1b[?1036r");
	tcsetattr(STDOUT_FILENO, TCSADRAIN, &saved);
}

static bool initX11(void) {
	int revert_to_return;

	if ((display = XOpenDisplay(NULL)) == NULL)
		return false;
	root = XDefaultRootWindow(display);
	XGetInputFocus(display, &focus_window, &revert_to_return);
	return true;
}

static void send_event(KeySym keysym, unsigned state) {
	XEvent event;

	event.xkey.type = KeyPress;
	event.xkey.serial = 0;
	event.xkey.send_event = 0;
	event.xkey.display = display;
	event.xkey.window = focus_window;
	event.xkey.subwindow = None;
	event.xkey.root = root;
	event.xkey.time = CurrentTime;
	event.xkey.x = 1;
	event.xkey.y = 1;
	event.xkey.x_root = 1;
	event.xkey.y_root = 1;
	event.xkey.state = state;
	event.xkey.keycode = XKeysymToKeycode(display, keysym);
	event.xkey.same_screen = True;

	XSendEvent(display, focus_window, True, KeyPressMask | KeyReleaseMask, &event);

	event.xkey.type = KeyRelease;
	event.xkey.serial = 0;
	event.xkey.send_event = 0;
	event.xkey.display = display;
	event.xkey.window = focus_window;
	event.xkey.subwindow = None;
	event.xkey.root = root;
	event.xkey.time = CurrentTime;
	event.xkey.x = 1;
	event.xkey.y = 1;
	event.xkey.x_root = 1;
	event.xkey.y_root = 1;
	event.xkey.state = state;
	event.xkey.keycode = XKeysymToKeycode(display, keysym);
	event.xkey.same_screen = True;

	XSendEvent(display, focus_window, True, KeyPressMask | KeyReleaseMask, &event);
	while (XPending(display))
		XNextEvent(display, &event);
}

static void init_terminal(void) {
	char *env;
	struct termios new_params;
	if (!isatty(STDOUT_FILENO))
		fatal("Stdout is not a terminal\n");

	//~ setupterm((char *)0, 1, (int *)0);

	if (tcgetattr(STDOUT_FILENO, &saved) < 0)
		fatal("Could not retrieve terminal settings: %s\n", strerror(errno));

	new_params = saved;
	new_params.c_iflag &= ~(IXON | IXOFF);
	new_params.c_iflag |= INLCR;
	new_params.c_lflag &= ~(ISIG | ICANON | ECHO);
	new_params.c_cc[VMIN] = 1;

	if (tcsetattr(STDOUT_FILENO, TCSADRAIN, &new_params) < 0)
		fatal("Could not change terminal settings: %s\n", strerror(errno));

	atexit(restore_terminal);
	FD_ZERO(&inset);
	FD_SET(STDIN_FILENO, &inset);
	env = getenv("TERM");
	#warning FIXME: this probably should not be here
	if (strncmp("xterm", env, 5) == 0) {
		printf("\x1b[?1036s\x1b[?1036h");
		xterm_restore = true;
	}

}

/* Read a char masking interruptions. */
static int safe_read_char(void) {
	char c;
	while (1) {
		ssize_t retval = read(STDIN_FILENO, &c, 1);
		if (retval < 0 && errno == EINTR)
			continue;
		else if (retval >= 1)
			return c;

		return -1;
	}
}

/* Read a character from the keyboard, with timeout. */
static int get_keychar(int msec) {
	int retval;
	fd_set _inset;
	struct timeval timeout;


	while (1) {
		_inset = inset;
		if (msec > 0) {
			timeout.tv_sec = msec / 1000;
			timeout.tv_usec = (msec % 1000) * 1000;
		}

		retval = select(STDIN_FILENO + 1, &_inset, NULL, NULL, msec > 0 ? &timeout : NULL);

		if (retval < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		} else if (retval == 0) {
			return -1;
		} else {
			return safe_read_char();
		}
	}
}

static bool confirm(const char *question) {
	int c;

	do {
		printf("%s? ", question);
		fflush(stdout);
		c = safe_read_char();
		if (c == 3) {
			printf("^C\n");
			exit(EXIT_FAILURE);
		}
		printf("\n");
	} while (c != 'y' && c != 'Y' && c != 'n' && c != 'N');

	return c == 'y' || c == 'Y';
}

/* Read a sequence_t from the keyboard. */
static sequence_t *get_sequence(void) {
	sequence_t *retval;
	char seq[MAX_SEQUENCE];
	int c, idx = 0;

	do {
		c = get_keychar(x11_auto ? 50 : -1);
		if (x11_auto && c < 0)
			return NULL;

		if (c == ESC) {
			int i, dest = 0;

			seq[idx++] = ESC;

			while ((c = get_keychar(KEY_TIMEOUT)) != -1 && idx < MAX_SEQUENCE)
				seq[idx++] = c;

			if (idx == MAX_SEQUENCE) {
				printf("sequence too long");
				return NULL;
			} else if (idx < 2) {
				printf("sequence too short");
				return NULL;
			}

			if ((retval = malloc(sizeof(sequence_t))) == NULL)
				fatal("Out of memory\n");

			for (i = 0; i < idx; i++) {
				if (seq[i] == '\\') {
					retval->seq[dest++] = '\\';
					retval->seq[dest++] = '\\';
				} else if (isprint(seq[i])) {
					retval->seq[dest++] = seq[i];
				} else if (seq[i] == 27) {
					retval->seq[dest++] = '\\';
					retval->seq[dest++] = 'e';
				} else {
					sprintf(retval->seq + dest, "\\%03o", seq[i]);
					dest += 4;
				}
			}
			retval->seq[dest] = 0;
			retval->duplicate = NULL;
			retval->remove = false;
			return retval;
		} else if (!x11_auto && c == 3) {
			printf("^C\n");
			if (confirm("Are you sure you want to quit"))
				exit(EXIT_FAILURE);
			printf("(no escape sequence)");
			return NULL;
		} else if (c == 0177) {
			if ((retval = malloc(sizeof(sequence_t))) == NULL)
				fatal("Out of memory\n");
			strcpy(retval->seq, "\\177");
			retval->duplicate = NULL;
			retval->remove = false;
			return retval;
		} else if (c == 18) {
			return (void *) -1;
		} else if (c >= 0) {
			while (get_keychar(KEY_TIMEOUT) >= 0) {}
			printf("(no escape sequence)");
			return NULL;
		}
	} while (c < 0);
	return NULL;
}

static sequence_t *head = NULL;
static FILE *output;

/* Get the set of key mappings for a single modifier combination.
   Mappings are appended to "head". Returns 0 on success and -1 on failure. */
static int getkeys(name_mapping_t *keys, int max, int mod) {
	sequence_t *current;
	int i;
	for (i = 0; i < max; i++) {
		sequence_t *new_seq;
		printf("%s%s ", modifiers[mod].name, keys[i].name);
		fflush(stdout);
		if (x11_auto)
			send_event(keys[i].keysym, modifiers[mod].keysym);

		new_seq = get_sequence();
		if (new_seq == NULL) {
			printf("\n");
			continue;
		} else if (new_seq == (void *) -1) {
			return -1;
		}
		printf("%s ", new_seq->seq);
		current = head;
		while (current != NULL) {
			if (strcmp(current->seq, new_seq->seq) == 0 && current->duplicate == NULL) {
				printf("(duplicate for %s%s)", current->modifiers->name, current->keynames->name);
				new_seq->duplicate = current;
				break;
			} else {
				current = current->next;
			}
		}
		new_seq->modifiers = modifiers + mod;
		new_seq->keynames = keys + i;
		new_seq->next = head;
		head = new_seq;
		printf("\n");
	}
	return 0;
}

static void write_keys(sequence_t *keys) {
	sequence_t *current;

	for (current = keys; current != NULL; current = current->next) {
		if (current->duplicate != NULL) {
			fprintf(output, "# %s%s = %s%s\n", current->keynames->identifier, current->modifiers->identifier,
				current->duplicate->keynames->identifier, current->duplicate->modifiers->identifier);
		} else {
			fprintf(output, "%s%s = \"%s\"\n", current->keynames->identifier, current->modifiers->identifier,
				current->seq);
		}
	}
}

static char *get_print_seq(const char *seq) {
	static char buffer[1024];
	char *dest = buffer;

	while (*seq && dest - buffer < 1018) {
		if (*seq == '"') {
			*dest++ = '\\';
			*dest++ = '"';
			seq++;
		} else if (*seq == '\\') {
			*dest++ = *seq;
			*dest++ = *seq++;
		} else if (isprint(*seq)) {
			*dest++ = *seq++;
		} else if (*seq == 27) {
			*dest++ = '\\';
			*dest++ = 'e';
			seq++;
		} else {
			sprintf(dest, "\\%03o", *seq++);
			dest += 4;
		}
	}
	*dest = 0;
	return buffer;
}

static char *parse_escapes(const char *seq) {
	char buffer[1024];
	int dest = 0;

	while (*seq && dest < 1023) {
		if (*seq == '\\') {
			seq++;
			switch (*seq) {
				case 'E':
				case 'e':
					buffer[dest++] = 27;
					seq++;
					break;
				case '"':
					buffer[dest++] = '"';
					seq++;
					break;
				case '\\':
					buffer[dest++] = '\\';
					seq++;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					buffer[dest] = *seq++ - '0';
					if (isdigit(*seq))
						buffer[dest] = buffer[dest] * 8 + *seq++ - '0';
					if (isdigit(*seq))
						buffer[dest] = buffer[dest] * 8 + *seq++ - '0';
					dest++;
					break;
				default:
					buffer[dest++] = *seq++;
					break;
			}
		} else {
			buffer[dest++] = *seq++;
		}
	}
	buffer[dest] = 0;
	return strdup(buffer);
}

static void write_map(FILE *output, map_t *mode, map_t *maps) {
	sequence_t *current, *last;

	if (mode->name != NULL) {
		fprintf(output, "%s {\n", mode->name);
	} else {
		map_list_t *ptr;
		for (ptr = mode->collected_from; ptr != NULL; ptr = ptr->next)
			fprintf(output, "_%s", ptr->map->name);
		fprintf(output, " {\n");
	}
	if (mode->esc_seq_enter) {
		if (mode->esc_seq_enter_name)
			fprintf(output, "%%enter = %s\n", mode->esc_seq_enter_name);
		else
			fprintf(output, "%%enter = \"%s\"\n", get_print_seq(mode->esc_seq_enter));
	}
	if (mode->esc_seq_leave) {
		if (mode->esc_seq_leave_name)
			fprintf(output, "%%leave = %s\n", mode->esc_seq_leave_name);
		else
			fprintf(output, "%%leave = \"%s\"\n", get_print_seq(mode->esc_seq_leave));
	}

	for (; maps != NULL; maps = maps->next) {
		map_list_t *collected;
		for (collected = maps->collected_from; collected != NULL; collected = collected->next) {
			if (collected->map == mode) {
				fprintf(output, "%%include ");
				for (collected = maps->collected_from; collected != NULL; collected = collected->next)
					fprintf(output, "_%s", collected->map->name);
				fprintf(output, "\n");
				break;
			}
		}
	}

	write_keys(mode->sequences);

	for (current = mode->sequences, last = NULL; current != NULL; last = current, current = current->next)
		free(last);

	fprintf(output, "}\n\n");
}

static sequence_t *reverse_key_list(sequence_t *list) {
	sequence_t *ptr, *current;
	current = list->next;
	list->next = NULL;
	while (current != NULL) {
		ptr = current->next;
		current->next = list;
		list = current;
		current = ptr;
	}
	return list;
}

static void learn_map(map_t *mode) {
	sequence_t *current, *before_insert;
	size_t i;

	printf("Starting mode %s\n", mode->name);
	if (mode->esc_seq_enter != NULL)
		putp(mode->esc_seq_enter);

	fflush(stdout);
	if (x11_auto)
		usleep(100000);

	for (i = 0; i < SIZEOF(modifiers); i++) {
		before_insert = head;
		if (getkeys(keynames, SIZEOF(keynames), i) < 0) {
			printf("\nRestarting...\n");
			goto skip;
		}
		if (getkeys(functionkeys, maxfkeys, i) < 0) {
			printf("\nRestarting...\n");
			goto skip;
		}

		if (!x11_auto && !confirm("Are you satisfied with the above keys")) {
	skip:
			while (head != before_insert) {
				current = head;
				head = current->next;
				free(current);
			}
			i--;
		}
	}

	mode->sequences = reverse_key_list(head);
	head = NULL;

	if (mode->esc_seq_leave != NULL)
		putp(mode->esc_seq_leave);
}

static void extract_shared_maps(map_t *head, map_t *last_new) {
	map_t *new_maps_head = NULL;
	map_t *current_map, *new_map;
	sequence_t *current_key, *search_for, *key, *last_key;

	for (current_map = head; current_map != NULL; current_map = current_map->next) {
		if (current_map == last_new)
			continue;

		new_map = NULL;
		for (search_for = last_new->sequences; search_for != NULL; search_for = search_for->next) {
			for (current_key = current_map->sequences; current_key != NULL; current_key = current_key->next) {
				if (search_for->keynames == current_key->keynames &&
						search_for->modifiers == current_key->modifiers &&
						strcmp(search_for->seq, current_key->seq) == 0)
				{
					if (new_map == NULL) {
						new_map = calloc(1, sizeof(map_t));
						new_map->collected_from = calloc(1, sizeof(map_list_t));
						new_map->collected_from->map = last_new;
						if (current_map->collected_from != NULL) {
							new_map->collected_from->next = current_map->collected_from;
						} else {
							new_map->collected_from->next = calloc(1, sizeof(map_list_t));
							new_map->collected_from->next->map = current_map;
						}
						new_map->next = new_maps_head;
						new_maps_head = new_map;
					}
					key = malloc(sizeof(sequence_t));
					*key = *search_for;
					key->next = new_map->sequences;
					new_map->sequences = key;
					search_for->remove = true;
					current_key->remove = true;
				}
			}
		}
		if (new_map != NULL)
			new_map->sequences = reverse_key_list(new_map->sequences);
	}

	for (current_map = head; current_map != NULL; current_map = current_map->next) {
		for (last_key = NULL, current_key = current_map->sequences; current_key != NULL; ) {
			if (current_key->remove) {
				key = current_key;
				current_key = current_key->next;
				if (last_key == NULL)
					current_map->sequences = current_key;
				else
					last_key->next = current_key;
			} else {
				last_key = current_key;
				current_key = current_key->next;
			}
		}
	}

	for (current_map = head; current_map->next != NULL; current_map = current_map->next) {}
	current_map->next = new_maps_head;
}

int main(int argc, char *argv[]) {
	size_t i;
	char *smkx = NULL, *rmkx = NULL;
	const char *term = getenv("TERM");
	map_t *mode_head = NULL, *mode_ptr, **mode_next, *last_mode_ptr;

	(void) argc;
	(void) argv;

	if (term == NULL)
		fatal("No terminal type has been set in the TERM environment variable\n");
	setupterm((char *)0, 1, (int *)0);

	if ((output = fopen(term, "w")) == NULL)
		fatal("Can't open output file '%s': %s\n", term, strerror(errno));

	x11_auto = initX11();

	printf("libt3key key learning program\n");
	if (x11_auto) {
		printf("Automatically learning keys for terminal %s. DO NOT press a key while the key learning is in progress.\n", term);
	} else {
		printf("Learning keys for terminal %s. Please press the requested key or enter\n", term);
		printf("WARNING: Be carefull when pressing combinations as they may be bound to actions\nyou don't want to execute! For best results don't run this in a window manager.\n");
	}

	do {
		printf("How many function keys does your keyboard have? ");
		scanf("%d", &maxfkeys);
	} while (maxfkeys < 0);

	if (maxfkeys > 0) {
		functionkeys = malloc(maxfkeys * sizeof(name_mapping_t));
		for (i = 0; (int) i < maxfkeys; i++) {
			char buffer[1024];
			snprintf(buffer, 1024, "F%u", (unsigned) i + 1);
			functionkeys[i].name = strdup(buffer);
			snprintf(buffer, 1024, "f%u", (unsigned) i + 1);
			functionkeys[i].identifier = strdup(buffer);
			functionkeys[i].keysym = XK_F1 + i;
		}
	}

	rmkx = tigetstr("rmkx");
	if (rmkx == (char *) -1 || strlen(rmkx) == 0)
		rmkx = NULL;
	mode_head = calloc(1, sizeof(map_t));
	mode_head->name = strdup("nokx");
	if (rmkx != NULL) {
		mode_head->esc_seq_enter = strdup(rmkx);
		mode_head->esc_seq_enter_name = strdup("rmkx");
	}

	mode_next = &mode_head->next;
	smkx = tigetstr("smkx");
	if (smkx != NULL && smkx != (void *) -1 && strlen(smkx) > 0) {
		(*mode_next) = calloc(1, sizeof(map_t));
		(*mode_next)->name = strdup("kx");
		(*mode_next)->esc_seq_enter = strdup(smkx);
		(*mode_next)->esc_seq_enter_name = strdup("smkx");
		(*mode_next)->esc_seq_leave = strdup(rmkx);
		(*mode_next)->esc_seq_leave_name = strdup("rmkx");
		mode_next = &(*mode_next)->next;
	}

	while (1) {
		char buffer[1024];
		printf("Name for extra mode (- to continue): ");
		scanf("%1023s", buffer);

		if (strcmp(buffer, "-") == 0)
			break;
		(*mode_next) = calloc(1, sizeof(map_t));
		(*mode_next)->name = strdup(buffer);
		do {
			printf("Escape sequence to enter mode %s (- for none): ", (*mode_next)->name);
			scanf("%1023s", buffer);
		} while (buffer[0] == 0);
		if (strcmp(buffer, "-") != 0)
			(*mode_next)->esc_seq_enter = parse_escapes(buffer);
		do {
			printf("Escape sequence to leave mode %s (- for none): ", (*mode_next)->name);
			scanf("%1023s", buffer);
		} while (buffer[0] == 0);
		if (strcmp(buffer, "-") != 0)
			(*mode_next)->esc_seq_leave = parse_escapes(buffer);
		mode_next = &(*mode_next)->next;
	}

	init_terminal();

	send_event(XK_a, 0);
	if (get_keychar(100) < 0)
		fatal("Sending keys does not work. Aborting.\n");

	for (mode_ptr = mode_head; mode_ptr != NULL && mode_ptr->name != NULL; mode_ptr = mode_ptr->next) {
		learn_map(mode_ptr);
		extract_shared_maps(mode_head, mode_ptr);
	}

	for (mode_ptr = mode_head, last_mode_ptr = NULL; mode_ptr != NULL; ) {
		if (mode_ptr->name == NULL && mode_ptr->sequences == NULL) {
			map_t *current = mode_ptr;
			mode_ptr = mode_ptr->next;
			if (last_mode_ptr == NULL)
				mode_head = mode_ptr;
			else
				last_mode_ptr->next = mode_ptr;
			free(current);
		} else {
			last_mode_ptr = mode_ptr;
			mode_ptr = mode_ptr->next;
		}
	}

	for (mode_ptr = mode_head; mode_ptr != NULL; mode_ptr = mode_ptr->next)
		write_map(output, mode_ptr, mode_head);


	fflush(output);
	fclose(output);

	if (x11_auto)
		XCloseDisplay(display);
	return 0;
}
