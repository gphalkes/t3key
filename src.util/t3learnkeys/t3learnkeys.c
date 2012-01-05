/* Copyright (C) 2011-2012 G.P. Halkes
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
#include <ctype.h>
#include <string.h>
#include <termios.h>
#ifdef HAS_SELECT_H
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <search.h>

#ifndef NO_AUTOLEARN
#include "x11.h"
#else
#define X_KEY_SYM(_x) 0
#endif

/* These must be below the X11 includes, because they define names that cause
   errors in the X11 XInput header file. */
/* Curses headers have bool type. */
#include <curses.h>
#include <term.h>

#include "optionMacros.h"

#define MAX_SEQUENCE 50
#define ESC 27
#define KEY_TIMEOUT 10

/* FIXME: test modifier-letter as well */

typedef struct {
	const char *name;
	const char *identifier;
	long keysym;
} name_mapping_t;

static name_mapping_t keynames[] = {
#ifndef TESTING
	{ "Insert", "insert", X_KEY_SYM(XK_Insert) },
	{ "Home", "home", X_KEY_SYM(XK_Home) },
	{ "Page Up", "page_up", X_KEY_SYM(XK_Page_Up) },
	{ "Delete", "delete", X_KEY_SYM(XK_Delete) },
	{ "End", "end", X_KEY_SYM(XK_End) },
	{ "Page Down", "page_down", X_KEY_SYM(XK_Page_Down) },
#endif
	{ "Up", "up", X_KEY_SYM(XK_Up) },
#ifndef TESTING
	{ "Left", "left", X_KEY_SYM(XK_Left) },
	{ "Down", "down", X_KEY_SYM(XK_Down) },
	{ "Right", "right", X_KEY_SYM(XK_Right) },
	{ "Keypad /", "kp_div", X_KEY_SYM(XK_KP_Divide) },
	{ "Keypad *", "kp_mul", X_KEY_SYM(XK_KP_Multiply) },
	{ "Keypad -", "kp_minus", X_KEY_SYM(XK_KP_Subtract) },
	{ "Keypad Home", "kp_home", X_KEY_SYM(XK_KP_Home) },
	{ "Keypad Up", "kp_up", X_KEY_SYM(XK_KP_Up) },
	{ "Keypad Page Up", "kp_page_up", X_KEY_SYM(XK_KP_Page_Up) },
	{ "Keypad +", "kp_plus", X_KEY_SYM(XK_KP_Add) },
	{ "Keypad Left", "kp_left", X_KEY_SYM(XK_KP_Left) },
	{ "Keypad Center", "kp_center", X_KEY_SYM(XK_KP_Begin) },
	{ "Keypad Right", "kp_right", X_KEY_SYM(XK_KP_Right) },
	{ "Keypad End", "kp_end", X_KEY_SYM(XK_KP_End) },
	{ "Keypad Down", "kp_down", X_KEY_SYM(XK_KP_Down) },
	{ "Keypad Page Down", "kp_page_down", X_KEY_SYM(XK_KP_Page_Down) },
	{ "Keypad Insert", "kp_insert", X_KEY_SYM(XK_KP_Insert) },
	{ "Keypad Delete", "kp_delete", X_KEY_SYM(XK_KP_Delete) },
	{ "Keypad Enter", "kp_enter", X_KEY_SYM(XK_KP_Enter) },
	{ "Tab", "tab", X_KEY_SYM(XK_Tab) },
	{ "Backspace", "backspace", X_KEY_SYM(XK_BackSpace) }
#endif
};

static name_mapping_t *functionkeys;

static name_mapping_t modifiers[] = {
	{ "", "", 0 },
#ifndef TESTING
	{ "Control ", "-c", X_KEY_SYM(ControlMask) },
	{ "Meta ", "-m", X_KEY_SYM(Mod1Mask) },
	{ "Shift ", "-s", X_KEY_SYM(ShiftMask) },
	{ "Control+Meta ", "-cm", X_KEY_SYM(ControlMask | Mod1Mask) },
	{ "Control+Shift ", "-cs", X_KEY_SYM(ControlMask | ShiftMask) },
	{ "Meta+Shift ", "-ms", X_KEY_SYM(Mod1Mask | ShiftMask) },
	{ "Control+Meta+Shift ", "-cms", X_KEY_SYM(ControlMask | Mod1Mask | ShiftMask) },
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

/* Use int, so we can use it safely in x11.c, which does not have the curses bool type. */
int option_auto_learn;
static bool option_no_abort_auto;
static const char *option_output;
static int option_no_duplicates;
static char **blocked_keys;
static size_t blocked_keys_fill;

static struct termios saved;
static fd_set inset;
static int maxfkeys = -1;

#ifndef NO_AUTOLEARN
int reprogram_code[4];
#endif
static sequence_t *head = NULL;
static FILE *output;


/** Alert the user of a fatal error and quit.
    @param fmt The format string for the message. See fprintf(3) for details.
    @param ... The arguments for printing.
*/
void fatal(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static void *safe_malloc(size_t size) {
	void *result;
	if ((result = malloc(size)) == NULL)
		fatal("Out of memory\n");
	return result;
}

static void *safe_calloc(size_t nmemb, size_t size) {
	void *result;
	if ((result = calloc(nmemb, size)) == NULL)
		fatal("Out of memory\n");
	return result;
}

static char *safe_strdup(const char *str) {
	char *result;
	if ((result = malloc(strlen(str) + 1)) == 0)
		fatal("Out of memory\n");
	strcpy(result, str);
	return result;
}

static void restore_terminal(void) {
	char *rmkx = tigetstr("rmkx");
	if (rmkx != NULL && rmkx != (void *) -1)
		printf("%s", rmkx);
	tcsetattr(STDOUT_FILENO, TCSADRAIN, &saved);
}

static void init_terminal(void) {
	char *env;
	struct termios new_params;
	if (!isatty(STDOUT_FILENO))
		fatal("Stdout is not a terminal\n");

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
		c = get_keychar(option_auto_learn ? 150 : -1);
		if (option_auto_learn && c < 0)
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

			retval = safe_malloc(sizeof(sequence_t));

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
		} else if (c == 3) {
			if (option_auto_learn) {
				if (!option_no_abort_auto)
					exit(EXIT_FAILURE);
			} else {
				printf("^C\n");
				if (confirm("Are you sure you want to quit [y/n]"))
					exit(EXIT_FAILURE);
			}
			c = -1;
		} else if (c == 0177 || c == 8) {
			retval = safe_malloc(sizeof(sequence_t));
			sprintf(retval->seq, "\\%03o", c);
			retval->duplicate = NULL;
			retval->remove = false;
			return retval;
		} else if (c == 18) { /* Control-r */
			return (void *) -1;
		} else if (c >= 0) {
			while (get_keychar(KEY_TIMEOUT) >= 0) {}
			printf("(no escape sequence)");
			return NULL;
		}
	} while (c < 0);
	return NULL;
}

/* Compare a key/modifier combination with a blocked key description. */
static int check_key(name_mapping_t *key_desc, const char **key_name) {
	const char *key_modifiers;
	size_t key_name_len, modifier_len = strlen(key_desc[1].name);

	/* First try if the user used the descriptive names that we print on screen... */
	if (strncmp(*key_name, key_desc[1].name, modifier_len) == 0 && strcmp((*key_name) + modifier_len, key_desc[0].name) == 0)
		return 0;

	/* ... and if not, check if it is one of the identifiers as used in the output file. */
	key_modifiers = strpbrk(*key_name, "+");
	key_name_len = key_modifiers == NULL ? strlen(*key_name) : (size_t) (key_modifiers - *key_name);
	if (strlen(key_desc[0].identifier) != key_name_len || memcmp(key_desc[0].identifier, *key_name, key_name_len) != 0)
		return 1;

	if (key_modifiers == NULL)
		return strcmp(key_desc[1].name, "");

	return strcmp(key_modifiers, key_desc[1].identifier);
}

/* Get the set of key mappings for a single modifier combination.
   Mappings are appended to "head". Returns 0 on success and -1 on failure. */
static int getkeys(name_mapping_t *keys, int max, int mod) {
	sequence_t *current, *new_seq;
	int i;

	for (i = 0; i < max; i++) {
		/* Check if the key is blocked from the command line. */
		if (blocked_keys != NULL) {
			name_mapping_t key_mod[2] = { keys[i], modifiers[mod] };
			if (lfind(&key_mod, blocked_keys, &blocked_keys_fill, sizeof(char *),
					(int (*)(const void *, const void *)) check_key) != NULL)
			{
				printf("Skipping blocked key %s%s\n", modifiers[mod].name, keys[i].name);
				continue;
			}
		}

		printf("%s%s ", modifiers[mod].name, keys[i].name);
		fflush(stdout);

#ifndef NO_AUTOLEARN
		/* When auto learning, just send the appropriate events to the terminal. */
		if (option_auto_learn)
			send_event(keys[i].keysym, modifiers[mod].keysym);
#endif

		/* Retrieve key sequence. */
		new_seq = get_sequence();
		if (new_seq == NULL) {
			printf("\n");
			continue;
		} else if (new_seq == (void *) -1) {
			return -1;
		}
		printf("%s ", new_seq->seq);
		current = head;
		/* Check for duplicate sequences, and set the duplicate field if one is found. */
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
			if (!option_no_duplicates) {
				fprintf(output, "\t\t# %s%s = %s%s\n", current->keynames->identifier, current->modifiers->identifier,
					current->duplicate->keynames->identifier, current->duplicate->modifiers->identifier);
			}
		} else {
			fprintf(output, "\t\t%s%s = \"%s\"\n", current->keynames->identifier, current->modifiers->identifier,
				current->seq);
		}
	}
}

/* Convert a sequence to a printable representation. */
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

/* Convert a string into a representation usable for writing to the terminal. */
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
	return safe_strdup(buffer);
}

/* Write a single map (mode) to the output. */
static void write_map(FILE *output, map_t *mode, map_t *maps) {
	sequence_t *current, *last;

	/* Mode name is NULL when the map is created by extracting common parts of
	   other maps. In that case the collected_from list is a linked list of map
	   pointers from which it was created. */
	if (mode->name != NULL) {
		fprintf(output, "\t%s {\n", mode->name);
	} else {
		map_list_t *ptr;
		fprintf(output, "\t");
		for (ptr = mode->collected_from; ptr != NULL; ptr = ptr->next)
			fprintf(output, "_%s", ptr->map->name);
		fprintf(output, " {\n");
	}

	if (mode->esc_seq_enter)
		fprintf(output, "\t\tenter = \"%s\"\n", mode->esc_seq_enter_name ?
			mode->esc_seq_enter_name : get_print_seq(mode->esc_seq_enter));

	if (mode->esc_seq_leave)
		fprintf(output, "\t\tleave = \"%s\"\n", mode->esc_seq_leave_name ?
			mode->esc_seq_leave_name : get_print_seq(mode->esc_seq_leave));

	/* Locate all the maps which contain common pieces collected from this map.
	   These have to be %include'd in this map. */
	for (; maps != NULL; maps = maps->next) {
		map_list_t *collected;
		for (collected = maps->collected_from; collected != NULL; collected = collected->next) {
			if (collected->map == mode) {
				fprintf(output, "\t\t%%use = \"");
				for (collected = maps->collected_from; collected != NULL; collected = collected->next)
					fprintf(output, "_%s", collected->map->name);
				fprintf(output, "\"\n");
				break;
			}
		}
	}

	write_keys(mode->sequences);

	for (current = mode->sequences, last = NULL; current != NULL; last = current, current = current->next)
		free(last);

	fprintf(output, "\t}\n\n");
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

/* Learn a single map. */
static void learn_map(map_t *mode) {
	sequence_t *current, *before_insert;
	size_t i;

	printf("Starting mode %s (press Control R to restart)\n", mode->name);
	if (mode->esc_seq_enter != NULL)
		putp(mode->esc_seq_enter);

	fflush(stdout);
	if (option_auto_learn)
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

		if (!option_auto_learn && !confirm("Are you satisfied with the above keys [y/n]")) {
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

/* Extract all shared parts of a new map, so we only save them once. */
static void extract_shared_maps(map_t *head, map_t *last_new) {
	map_t *new_maps_head = NULL;
	map_t *current_map, *new_map;
	sequence_t *current_key, *search_for, *key, *last_key;

	/* The newly created partial maps are created in a separate list, which is
	   linked to the main list at the end of this function. This makes it easier
	   to reason about the list of maps. */

	for (current_map = head; current_map != NULL; current_map = current_map->next) {
		if (current_map == last_new)
			continue;

		new_map = NULL;
		/* Note: we only compare the sequences from last_new with the sequences in
		   all other maps. This is _not_ an all to all compare. */
		for (search_for = last_new->sequences; search_for != NULL; search_for = search_for->next) {
			for (current_key = current_map->sequences; current_key != NULL; current_key = current_key->next) {
				/* We can avoid using strcmp here, because they all point into the
				   static arrays with descriptions. */
				if (search_for->keynames == current_key->keynames &&
						search_for->modifiers == current_key->modifiers &&
						strcmp(search_for->seq, current_key->seq) == 0 &&
						((search_for->duplicate == NULL) == (current_key->duplicate == NULL)))
				{
					if (new_map == NULL) {
						new_map = safe_calloc(1, sizeof(map_t));
						new_map->collected_from = safe_calloc(1, sizeof(map_list_t));
						new_map->collected_from->map = last_new;
						if (current_map->collected_from != NULL) {
							new_map->collected_from->next = current_map->collected_from;
						} else {
							new_map->collected_from->next = safe_calloc(1, sizeof(map_list_t));
							new_map->collected_from->next->map = current_map;
						}
						new_map->next = new_maps_head;
						new_maps_head = new_map;
					}
					key = safe_malloc(sizeof(sequence_t));
					*key = *search_for;
					key->next = new_map->sequences;
					new_map->sequences = key;
					/* Mark keys for later removal. Removing here would interfere with the
					   for-loop conditions and update steps. */
					search_for->remove = true;
					current_key->remove = true;
				}
			}
		}
		/* Keys are inserted at the front, because that takes less book-keeping. But
		   in the output we still want them in the original order, so reverse them here. */
		if (new_map != NULL)
			new_map->sequences = reverse_key_list(new_map->sequences);
	}

	/* Remove all keys that have been marked for removal. */
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

	/* Link the newly created maps the the end of the maps list. */
	for (current_map = head; current_map->next != NULL; current_map = current_map->next) {}
	current_map->next = new_maps_head;
}

PARSE_FUNCTION(parse_args)
	OPTIONS
#ifndef NO_AUTOLEARN
		OPTION('a', "auto-learn", NO_ARG)
			option_auto_learn = true;
		END_OPTION
		OPTION('A', "dont-abort-auto-learn", NO_ARG)
			option_no_abort_auto = true;
		END_OPTION
#endif
		OPTION('b', "block-keys", REQUIRED_ARG)
			char *comma = strchr(optArg, ',');
			size_t extra_blocked_keys = 1;
			while (comma != NULL) {
				extra_blocked_keys++;
				comma = strchr(comma + 1, ',');
			}
			blocked_keys = realloc(blocked_keys, (blocked_keys_fill + extra_blocked_keys) * sizeof(char *));
			if (blocked_keys == NULL)
				fatal("Out of memory\n");
			for (comma = strtok(optArg, ","); comma != NULL; comma = strtok(NULL, ","))
				blocked_keys[blocked_keys_fill++] = comma;
		END_OPTION
		OPTION('o', "output", REQUIRED_ARG)
			option_output = optArg;
		END_OPTION
		OPTION('h', "help", NO_ARG)
			printf("Usage: t3learnkeys [<options>]\n"
#ifndef NO_AUTOLEARN
				"  -a,--auto-learn             Learn by emulating key events for X11 terminals\n"
				"  -A,--dont-abort-auto-learn  Do abort on C-c when auto-learning\n"
#endif
				"  -b<keys>,--block-keys=<keys>  Do not ask for keys described in <keys>\n"
				"  -d,--no-duplicates          Do not print comments for duplicates\n"
				"  -o<name>,--output=<name>    Write output to <name> (default: $TERM)\n"
			);
			exit(EXIT_SUCCESS);
		END_OPTION
		DOUBLE_DASH
			NO_MORE_OPTIONS;
		END_OPTION

		fatal("Unknown option " OPTFMT "\n", OPTPRARG);
	NO_OPTION
		fatal("Extra arguments on command line: %s\n", optcurrent);
	END_OPTIONS
END_FUNCTION

#ifndef NO_AUTOLEARN
static void cleanup_keys(void) {
	if (option_auto_learn && reprogram_code[0] != -1) {
		/* Clear out the mapping we made. */
		KeySym keysym = NoSymbol;
		int i;
		for (i = 0; i < 4; i++)
			XChangeKeyboardMapping(display, reprogram_code[i], 1, &keysym, 1);
		XCloseDisplay(display);
	}
}
#endif


int main(int argc, char *argv[]) {
	char *smkx = NULL, *rmkx = NULL;
	const char *term = getenv("TERM"), *display_env = getenv("DISPLAY");
	map_t *mode_head = NULL, *mode_ptr, **mode_next, *last_mode_ptr;
	int error;
	size_t i;

	parse_args(argc, argv);

	if (term == NULL)
		fatal("No terminal type has been set in the TERM environment variable\n");
	setupterm(NULL, 1, &error);

#ifndef NO_AUTOLEARN
	if (option_auto_learn && !initX11())
		fatal("Failed to initialize X11 connection. Try running without -a/--auto-learn.\n");
	atexit(cleanup_keys);
#endif

	if (option_auto_learn) {
		printf("Automatically learning keys for terminal %s. DO NOT press a key while the key learning is in progress.\n", term);
		maxfkeys = 35;
	} else {
		printf("Learning keys for terminal %s. Please press the requested key or enter\n", term);
		printf("WARNING: Be carefull when pressing combinations as they may be bound to actions\n"
			"you don't want to execute! For best results don't run this in a window manager.\n");
		if (display_env != NULL && strlen(display_env) > 0)
			printf("\nHINT: It appears you are using an X11 terminal emulator. "
				"Perhaps using the --auto-learn option would be helpful.\n\n");
		do {
			printf("How many function keys does your keyboard have? ");
			scanf("%d", &maxfkeys);
		} while (maxfkeys < 0);
	}

	printf("Make sure that NumLock is disabled before starting\n");

	if (option_output == NULL)
		option_output = term;
	if ((output = fopen(option_output, "w")) == NULL)
		fatal("Can't open output file '%s': %s\n", option_output, strerror(errno));

	/* Set up functionkeys list. */
	if (maxfkeys > 0) {
		functionkeys = safe_malloc(maxfkeys * sizeof(name_mapping_t));
		for (i = 0; (int) i < maxfkeys; i++) {
			char buffer[1024];
			snprintf(buffer, 1024, "F%u", (unsigned) i + 1);
			functionkeys[i].name = safe_strdup(buffer);
			snprintf(buffer, 1024, "f%u", (unsigned) i + 1);
			functionkeys[i].identifier = safe_strdup(buffer);
			functionkeys[i].keysym = X_KEY_SYM(XK_F1 + i);
		}
	}

	/* Figure out whether this terminal can switch between keypad-transmit and
	   non-keypad-transmit modes. If so add the rmkx sequence as %enter sequence
	   for the non-keypad-transmit mode. */
	rmkx = tigetstr("rmkx");
	if (rmkx == (char *) -1 || rmkx == NULL || strlen(rmkx) == 0)
		rmkx = NULL;
	mode_head = safe_calloc(1, sizeof(map_t));
	mode_head->name = safe_strdup("nokx");
	if (rmkx != NULL) {
		mode_head->esc_seq_enter = safe_strdup(rmkx);
		mode_head->esc_seq_enter_name = safe_strdup("rmkx");
	}

	/* If the terminal has a keypad-transmit mode, add that with the smkx sequence
	   as the %enter sequence. */
	mode_next = &mode_head->next;
	smkx = tigetstr("smkx");
	if (smkx != NULL && smkx != (void *) -1 && strlen(smkx) > 0) {
		(*mode_next) = safe_calloc(1, sizeof(map_t));
		(*mode_next)->name = safe_strdup("kx");
		(*mode_next)->esc_seq_enter = safe_strdup(smkx);
		(*mode_next)->esc_seq_enter_name = safe_strdup("smkx");
		(*mode_next)->esc_seq_leave = safe_strdup(rmkx);
		(*mode_next)->esc_seq_leave_name = safe_strdup("rmkx");
		mode_next = &(*mode_next)->next;
	}

	/* Ask the user if he wants other modes apart from (non-)keypad-transmit. */
	while (1) {
		char buffer[1024];
		printf("Name for extra mode (- to continue): ");
		scanf("%1023s", buffer);

		if (strcmp(buffer, "-") == 0)
			break;
		(*mode_next) = safe_calloc(1, sizeof(map_t));
		(*mode_next)->name = safe_strdup(buffer);
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

	/* Setup the terminal in non-echo, wait for single keystroke mode. */
	init_terminal();

#ifndef NO_AUTOLEARN
	/* Test if sending key events to the terminal results in characters being typed. */
	if (option_auto_learn) {
		send_event(XK_a, 0);
		if (get_keychar(100) < 0)
			fatal("Sending keys does not work. You may have to configure your terminal to accept SendEvents. Aborting.\n");
	}
#endif

	/* Learn the different maps. */
	for (mode_ptr = mode_head; mode_ptr != NULL && mode_ptr->name != NULL; mode_ptr = mode_ptr->next) {
		learn_map(mode_ptr);
		/* Extract the common parts. */
		extract_shared_maps(mode_head, mode_ptr);
	}

	/* Remove empty maps. These may have been created by extracting shared maps. */
	for (mode_ptr = mode_head, last_mode_ptr = NULL; mode_ptr != NULL; ) {
		if (mode_ptr->name == NULL && mode_ptr->sequences == NULL) {
			map_t *current = mode_ptr;
			mode_ptr = mode_ptr->next;
			if (last_mode_ptr == NULL)
				mode_head = mode_ptr;
			else
				last_mode_ptr->next = mode_ptr;
			/*FIXME: shouldn't we also be freeing out the collected_from lists? */
			free(current);
		} else {
			last_mode_ptr = mode_ptr;
			mode_ptr = mode_ptr->next;
		}
	}

	fprintf(output, "format = 1\n");
	fprintf(output, "best = \"nokx\" # Replace this with the actual best map name\n\n");
	fprintf(output, "maps {\n");
	/* Write the output maps. */
	for (mode_ptr = mode_head; mode_ptr != NULL; mode_ptr = mode_ptr->next)
		write_map(output, mode_ptr, mode_head);

	fprintf(output, "}\n");
	fflush(output);
	fclose(output);

	return EXIT_SUCCESS;
}
