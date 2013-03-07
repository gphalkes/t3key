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
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <curses.h>
#include <term.h>
#include <t3config/config.h>

#ifdef USE_GETTEXT
#include <libintl.h>
#define _(x) dgettext("LIBT3", (x))
#else
#define _(x) (x)
#endif

#define T3_KEY_CONST
#include "key.h"

#ifndef DB_DIRECTORY
#define DB_DIRECTORY "/usr/local/share/libt3key"
#endif

#define MAX_VERSION 0

#include "shareddefs.h"

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
#define RETURN_ERROR(_e) do { if (error != NULL) *error = _e; goto return_error; } while (0)

#define ENSURE(_x) do { int _error = (_x); \
	if (_error == T3_ERR_SUCCESS) \
		break; \
	if (error != NULL) \
		*error = _error; \
	goto return_error; \
} while (0)

static t3_key_node_t *load_ti_keys(const char *term, int *error);
/* FIXME: make a function specific for each type (there seems to be only one anyway). */
static int new_node(void **result, size_t size);
#define NEW_NODE(_x) new_node((void **) (_x), sizeof(**(_x)))

#ifdef HAS_STRDUP
#define _t3_key_strdup strdup
#else
static char *_t3_key_strdup(const char *str) {
	char *result;
	size_t len = strlen(str) + 1;

	if ((result = malloc(len)) == NULL)
		return NULL;
	memcpy(result, str, len);
	return result;
}
#endif

#define is_asciidigit(x) ((x) >= '0' && (x) <= '9')
#define is_asciixdigit(x) (is_asciidigit(x) || ((x) >= 'a' && (x) <= 'f') || ((x) >= 'A' && (x) <= 'F'))
#define to_asciilower(x) (((x) >= 'A' && (x) <= 'F') ? (x) - 'A' + 'a' : (x))

static const char map_schema[] = {
#include "map.bytes"
};

/** Convert a string from the input format to an internally usable string.
	@param string A @a Token with the string to be converted.
	@return The length of the resulting string.

	The use of this function processes escape characters. The converted
	characters are written in the original string.
*/
static size_t parse_escapes(char *string) {
	size_t max_read_position = strlen(string);
	size_t read_position = 0, write_position = 0;
	size_t i;

	while(read_position < max_read_position) {
		if (string[read_position] == '\\') {
			read_position++;

			if (read_position == max_read_position)
				return 0;

			switch(string[read_position++]) {
				case 'E':
				case 'e':
					string[write_position++] = '\033';
					break;
				case 'n':
					string[write_position++] = '\n';
					break;
				case 'r':
					string[write_position++] = '\r';
					break;
				case '\'':
					string[write_position++] = '\'';
					break;
				case '\\':
					string[write_position++] = '\\';
					break;
				case 't':
					string[write_position++] = '\t';
					break;
				case 'b':
					string[write_position++] = '\b';
					break;
				case 'f':
					string[write_position++] = '\f';
					break;
				case 'a':
					string[write_position++] = '\a';
					break;
				case 'v':
					string[write_position++] = '\v';
					break;
				case '?':
					string[write_position++] = '\?';
					break;
				case '"':
					string[write_position++] = '"';
					break;
				case 'x': {
					/* Hexadecimal escapes */
					unsigned int value = 0;
					/* Read at most two characters, or as many as are valid. */
					for (i = 0; i < 2 && (read_position + i) < max_read_position &&
							is_asciixdigit(string[read_position + i]); i++)
					{
						value <<= 4;
						if (is_asciidigit(string[read_position + i]))
							value += (int) (string[read_position + i] - '0');
						else
							value += (int) (to_asciilower(string[read_position + i]) - 'a') + 10;
						if (value > UCHAR_MAX)
							return 0;
					}
					read_position += i;

					if (i == 0)
						return 0;

					string[write_position++] = (char) value;
					break;
				}
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7': {
					/* Octal escapes */
					int value = (int)(string[read_position - 1] - '0');
					size_t max_idx = string[read_position - 1] < '4' ? 2 : 1;
					for (i = 0; i < max_idx && read_position + i < max_read_position &&
							string[read_position + i] >= '0' && string[read_position + i] <= '7'; i++)
						value = value * 8 + (int)(string[read_position + i] - '0');

					read_position += i;

					string[write_position++] = (char) value;
					break;
				}
				default:
					string[write_position++] = string[read_position - 1];
					break;
			}
		} else {
			string[write_position++] = string[read_position++];
		}
	}
	/* Terminate string. */
	string[write_position] = 0;
	return write_position;
}

static t3_config_t *load_map_config(const char *term, int *error) {
	const char *path[3] = { NULL, DB_DIRECTORY, NULL };
	char *xdg_path = NULL;
	t3_config_error_t config_error;
	t3_config_t *map_config = NULL;
	t3_config_schema_t *schema = NULL;
	FILE *input = NULL;
	/* Variable to hold an alternate value to use for term, in case of TERM
	   weirdness (like screen.rxvt). */
	const char *search_term = NULL;

	/* Setup path. */
	path[0] = xdg_path = t3_config_xdg_get_path(T3_CONFIG_XDG_DATA_HOME, "libt3key", 0);

	/* Screen is a nasty beast. It generates its TERM setting on the fly. The main
	   variation is by terminal. So there is screen.rxvt, screen.Eterm etc.
	   Furthermore, there are all kinds of variants for colors and options, like
	   screen-256color and screen-bce. So we simply fall back to loading the
	   screen definition. */
	if (strncmp(term, "screen", 6) == 0 && (term[6] == '.' || term[6] == '-'))
		search_term = "screen";

	if ((input = t3_config_open_from_path(path[0] == NULL ? path + 1 : path,
			search_term == NULL ? term : search_term, T3_CONFIG_CLEAN_NAME)) == NULL)
		RETURN_ERROR(T3_ERR_ERRNO);

	if ((map_config = t3_config_read_file(input, &config_error, NULL)) == NULL)
		RETURN_ERROR(config_error.error);

	if ((schema = t3_config_read_schema_buffer(map_schema, sizeof(map_schema), &config_error, NULL)) == NULL)
		RETURN_ERROR(config_error.error);

	if (!t3_config_validate(map_config, schema, &config_error, 0))
		RETURN_ERROR(config_error.error);

	free(xdg_path);
	fclose(input);
	t3_config_delete_schema(schema);

	return map_config;
return_error:
	if (input != NULL)
		fclose(input);
	free(xdg_path);
	t3_config_delete(map_config);
	t3_config_delete_schema(schema);
	return NULL;
}

static int convert_map(t3_config_t *map_config, t3_config_t *ptr, t3_key_node_t **next, t3_bool outer) {
	for (ptr = t3_config_get(ptr, NULL); ptr != NULL; ptr = t3_config_get_next(ptr)) {
		const char *name = t3_config_get_name(ptr);
		if (strcmp(name, "_use") == 0) {
			t3_config_t *use;
			/* Prevent infinte recursion and double inclusion by unlinking the map
			   from the list. */
			int result;

			for (use = t3_config_get(ptr, NULL); use != NULL; use = t3_config_get_next(use)) {
				t3_config_t *use_map = t3_config_unlink(t3_config_get(map_config, "maps"), t3_config_get_string(use));

				if (use_map == NULL)
					continue;
				result = convert_map(map_config, use_map, next, t3_false);
				t3_config_delete(use_map);
				if (result != T3_ERR_SUCCESS)
					return result;
				/* Update next pointer, because loading the sub-map will have extended
				   the list. */
				while (*next != NULL)
					next = &(*next)->next;
			}
		} else if (name[0] != '_' || strcmp(name, "_enter") == 0 || strcmp(name, "_leave") == 0) {
			if ((*next = malloc(sizeof(t3_key_node_t))) == NULL)
				return t3_false;
			(*next)->string = NULL;
			(*next)->next = NULL;

			if (((*next)->key = _t3_key_strdup(name)) == NULL)
				return T3_ERR_OUT_OF_MEMORY;

			/* Only for _enter and _leave, the name starts with _. */
			if (name[0] == '_' && t3_config_get_string(ptr)[0] != '\\') {
				/* Get terminfo string indicated by string. */
				const char *ti_string;

				if (!outer)
					return T3_ERR_INVALID_FORMAT;

				ti_string = tigetstr(t3_config_get_string(ptr));
				if (ti_string == (char *) 0 || ti_string == (char *) -1) {
					free((*next)->key);
					free(*next);
					*next = NULL;
					continue;
				}
				if (((*next)->string = _t3_key_strdup(ti_string)) == NULL)
					return T3_ERR_OUT_OF_MEMORY;
				(*next)->string_length = strlen((*next)->string);
			} else {
				(*next)->string = t3_config_take_string(ptr);
				(*next)->string_length = parse_escapes((*next)->string);
				if ((*next)->string_length == 0)
					return T3_ERR_INVALID_FORMAT;
			}
			next = &(*next)->next;
		}
	}
	return T3_ERR_SUCCESS;
}


t3_key_node_t *t3_key_load_map(const char *term, const char *map_name, int *error) {
	t3_config_t *map_config = NULL, *ptr;
	t3_key_node_t *list = NULL, *node = NULL;
	int result;

	if (term == NULL) {
		term = getenv("TERM");
		if (term == NULL)
			RETURN_ERROR(T3_ERR_NO_TERM);
	}

	if ((map_config = load_map_config(term, &result)) == NULL) {
		if (result == T3_ERR_ERRNO && errno == ENOENT)
			return load_ti_keys(term, error);
		RETURN_ERROR(result);
	}

	if (map_name != NULL)
		ptr = t3_config_unlink(t3_config_get(map_config, "maps"), map_name);
	else
		ptr = t3_config_unlink(t3_config_get(map_config, "maps"), t3_config_get_string(t3_config_get(map_config, "best")));

	if (ptr == NULL)
		RETURN_ERROR(T3_ERR_NOMAP);

	result = convert_map(map_config, ptr, &list, t3_true);
	t3_config_delete(ptr);
	if (result != T3_ERR_SUCCESS)
		RETURN_ERROR(result);

	if ((ptr = t3_config_get(map_config, "shiftfn")) != NULL) {
		if ((node = malloc(sizeof(t3_key_node_t))) == NULL)
			RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);
		node->string = NULL;
		node->next = NULL;
		if ((node->key = _t3_key_strdup("_shiftfn")) == NULL)
			RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);
		if ((node->string = malloc(3)) == NULL)
			RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);
		node->string_length = 3;
		ptr = t3_config_get(ptr, NULL);
		node->string[0] = t3_config_get_int(ptr);
		ptr = t3_config_get_next(ptr);
		node->string[1] = t3_config_get_int(ptr);
		ptr = t3_config_get_next(ptr);
		node->string[2] = t3_config_get_int(ptr);
		node->next = list;
		list = node;
		node = NULL;
	}

	if (t3_config_get_bool(t3_config_get(map_config, "xterm_mouse"))) {
		if ((node = malloc(sizeof(t3_key_node_t))) == NULL)
			RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);
		node->string = NULL;
		node->string_length = 0;
		node->next = NULL;
		if ((node->key = _t3_key_strdup("_xterm_mouse")) == NULL)
			RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);
		node->next = list;
		list = node;
		node = NULL;
	}
	t3_config_delete(map_config);
	return list;

return_error:
	t3_key_free_map(node);
	t3_key_free_map(list);
	t3_config_delete(map_config);
	return NULL;
}


void t3_key_free_map(t3_key_node_t *list) {
	t3_key_node_t *prev;
	while (list != NULL) {
		prev = list;
		list = list->next;
		free(prev->key);
		free(prev->string);
		free(prev);
	}
}

static int new_node(void **result, size_t size) {
	if ((*result = malloc(size)) == NULL)
		return T3_ERR_OUT_OF_MEMORY;
	/* FIXME: this causes trouble if the data in *result contains pointers */
	memset(*result, 0, size);
	return T3_ERR_SUCCESS;
}

static int make_node_from_ti(t3_key_node_t **next_node, const char *tikey, const char *key) {
	char *tiresult;
	int error;

	tiresult = tigetstr(tikey);
	if (tiresult == (char *)0 || tiresult == (char *)-1)
		return T3_ERR_SUCCESS;

	if ((error = NEW_NODE(next_node)) != T3_ERR_SUCCESS)
		return error;

	if (((*next_node)->key = _t3_key_strdup(key)) == NULL) {
		free(*next_node);
		*next_node = NULL;
		return T3_ERR_OUT_OF_MEMORY;
	}

	if (((*next_node)->string = _t3_key_strdup(tiresult)) == NULL) {
		free((*next_node)->string);
		free(*next_node);
		*next_node = NULL;
		return T3_ERR_OUT_OF_MEMORY;
	}
	(*next_node)->string_length = strlen(tiresult);

	return T3_ERR_SUCCESS;
}

/* The START/END MAPPINGS comments below are markers to allow extraction of this
   list for t3keyc. */

/*START MAPPINGS*/
typedef struct {
	const char *tikey;
	const char *key;
} mapping_t;

static const mapping_t keymapping[] = {
	{ "kich1", "insert" },
	{ "kdch1", "delete" },
	{ "khome", "home" },
	{ "kend", "end" },
	{ "kpp", "page_up" },
	{ "knp", "page_down" },
	{ "kcuu1", "up" },
	{ "kcub1", "left" },
	{ "kcud1", "down" },
	{ "kcuf1", "right" },
	{ "ka1", "kp_home" },
	{ "kc1", "kp_end" },
	{ "kb2", "kp_center" },
	{ "ka3", "kp_page_up" },
	{ "kc3", "kp_page_down" },
	{ "kbs", "backspace" },

	{ "kIC", "insert-s" },
	{ "kDC", "delete-s" },
	{ "kHOM", "home-s" },
	{ "kEND", "end-s" },
	{ "kPRV", "page_up-s" },
	{ "kNXT", "page_down-s" },
	{ "kLFT", "left-s" },
	{ "kRIT", "right-s" },
	{ "kcbt", "tab-s" },
	{ "kent", "enter" },
	{ "kind", "down-s" },
	{ "kri", "up-s" }
};
/*END MAPPINGS*/

static t3_key_node_t *load_ti_keys(const char *term, int *error) {
	t3_key_node_t *list = NULL, **next_node = &list;
	char function_key[10];
	size_t i;
	int errret, j;

	if (setupterm(term, 1, &errret) == ERR) {
		if (errret == 1)
			RETURN_ERROR(T3_ERR_HARDCOPY_TERMINAL);
		else if (errret == -1)
			RETURN_ERROR(T3_ERR_TERMINFODB_NOT_FOUND);
		else if (errret == 0)
			RETURN_ERROR(T3_ERR_TERMINAL_TOO_LIMITED);
		RETURN_ERROR(T3_ERR_UNKNOWN);
	}

	ENSURE(make_node_from_ti(next_node, "smkx", "_enter"));
	if (*next_node != NULL)
		next_node = &(*next_node)->next;
	ENSURE(make_node_from_ti(next_node, "rmkx", "_leave"));
	if (*next_node != NULL)
		next_node = &(*next_node)->next;

	for (i = 0; i < ARRAY_LENGTH(keymapping); i++) {
		ENSURE(make_node_from_ti(next_node, keymapping[i].tikey, keymapping[i].key));
		if (*next_node != NULL)
			next_node = &(*next_node)->next;
	}

	for (j = 1; j < 64; j++) {
		sprintf(function_key, "kf%d", j);
		ENSURE(make_node_from_ti(next_node, function_key, function_key + 1));
		if (*next_node == NULL)
			break;
		next_node = &(*next_node)->next;
	}
	return list;

return_error:
	t3_key_free_map(list);
	return NULL;
}

t3_key_string_list_t *t3_key_get_map_names(const char *term, int *error) {
	t3_config_t *map_config = NULL, *ptr;
	t3_key_string_list_t *list = NULL, *item;

	if ((map_config = load_map_config(term, error)) == NULL)
		return NULL;

	for (ptr = t3_config_get(t3_config_get(map_config, "maps"), NULL); ptr != NULL; ptr = t3_config_get_next(ptr)) {
		if (t3_config_get_name(ptr)[0] == '_')
			continue;
		if ((item = malloc(sizeof(t3_key_string_list_t))) == NULL)
			RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);

		if ((item->string = _t3_key_strdup(t3_config_get_name(ptr))) == NULL) {
			free(item);
			RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);
		}
		item->next = list;
		list = item;
	}
	t3_config_delete(map_config);
	return list;
return_error:
	t3_config_delete(map_config);
	return NULL;
}

void t3_key_free_names(t3_key_string_list_t *list) {
	t3_key_string_list_t *prev;
	while (list != NULL) {
		prev = list;
		list = list->next;
		free(prev->string);
		free(prev);
	}
}

char *t3_key_get_best_map_name(const char *term, int *error) {
	t3_config_t *map_config = NULL;
	char *best = NULL;
	if ((map_config = load_map_config(term, error)) == NULL)
		return NULL;

	if ((best = _t3_key_strdup(t3_config_get_string(t3_config_get(map_config, "best")))) == NULL) {
		if (error != NULL)
			*error = T3_ERR_OUT_OF_MEMORY;
	}

	t3_config_delete(map_config);
	return best;
}

t3_key_node_t *t3_key_get_named_node(T3_KEY_CONST t3_key_node_t *map, const char *name) {
	if (name == NULL) {
		if (map == NULL)
			return NULL;
		name = map->key;
		map = map->next;
	}

	for (; map != NULL; map = map->next) {
		if (strcmp(map->key, name) == 0)
			return map;
	}
	return NULL;
}

long t3_key_get_version(void) {
	return T3_KEY_VERSION;
}

const char *t3_key_strerror(int error) {
	switch (error) {
		default:
			return t3_config_strerror(error);

		case T3_ERR_INVALID_FORMAT:
			return _("invalid key-database file format");
		case T3_ERR_TERMINFO_UNKNOWN:
			return _("required terminfo key not found in terminfo database");
		case T3_ERR_NOMAP:
			return _("key database contains no maps");
		case T3_ERR_TRUNCATED_DB:
			return _("key database is truncated");
		case T3_ERR_READ_ERROR:
			return _("error reading key database");
		case T3_ERR_WRONG_VERSION:
			return _("key database is of an unsupported version");
	}
}
