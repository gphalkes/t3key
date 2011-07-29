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
#include <string.h>
#include <curses.h>
#include <errno.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <term.h>

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

#define RETURN_ERROR(_e) do { if (error != NULL) *error = _e; goto return_error; } while (0)
#define CLEANUP() do { free(best_map); free(current_map); fclose(input); } while (0)
#define CLEANUP_RETURN_ERROR(_e) do { CLEANUP(); RETURN_ERROR(_e); } while (0)
#define ENSURE(_x) do { int _error = (_x); \
	if (_error == T3_ERR_SUCCESS) \
		break; \
	if (error != NULL) \
		*error = _error; \
	goto return_error; \
} while (0)
#define EOF_OR_ERROR(_file) (feof(_file) ? T3_ERR_TRUNCATED_DB : T3_ERR_READ_ERROR)

static int check_magic_and_version(FILE *input);
static int skip_string(FILE *input);
static int read_string(FILE *input, char **string, size_t *length_ptr);
static int new_node(void **result, size_t size);
#define NEW_NODE(_x) new_node((void **) (_x), sizeof(**(_x)))
static t3_key_node_t *load_ti_keys(const char *term, int *error);

#ifdef HAS_STRDUP
#define strdup_impl strdup
#else
static char *strdup_impl(const char *str) {
	char *result;
	size_t len = strlen(str) + 1;

	if ((result = malloc(len)) == NULL)
		return NULL;
	memcpy(result, str, len);
	return result;
}
#endif

static char *make_name(const char *directory, const char *term) {
	size_t name_length, term_length;
	char *name;

	term_length = strlen(term);
	name_length = strlen(directory) + 3 + term_length + 1;
	if ((name = malloc(name_length)) == NULL)
		return NULL;

	strcpy(name, directory);
	strcat(name, "/");
	strncat(name, term, 1);
	strcat(name, "/");
	strncat(name, term, term_length);
	name[name_length - 1] = 0;
	return name;
}

/** Open database.
    @param term The terminal name to use or @c NULL for contents of @c TERM.
    @param error The location to store an error.
    @return A pointer to the @c FILE for the database, or @c NULL or error.
*/
static FILE *open_database(const char *term, int *error) {
	char *name, *db_directory_env, *home_env;
	FILE *input = NULL;

	if (term == NULL) {
		term = getenv("TERM");
		if (term == NULL)
			RETURN_ERROR(T3_ERR_NO_TERM);
	}

	home_env = getenv("HOME");
	if (home_env != NULL) {
		if ((name = malloc(strlen(home_env) + strlen(".libt3key") + 2)) == NULL)
			RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);
		strcpy(name, home_env);
		strcat(name, "/");
		strcat(name, ".libt3key");
		home_env = name;

		if ((name = make_name(home_env, term)) == NULL) {
			free(home_env);
			RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);
		}
		free(home_env);

		if ((input = fopen(name, "rb")) != NULL) {
			free(name);
			return input;
		}
		free(name);
	}

	db_directory_env = getenv("LIBT3KEY_DATABASE_DIR");
	if (db_directory_env != NULL) {
		if ((name = make_name(db_directory_env, term)) == NULL)
			RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);

		if ((input = fopen(name, "rb")) != NULL) {
			free(name);
			return input;
		}
		free(name);
	}

	if ((name = make_name(DB_DIRECTORY, term)) == NULL)
		RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);

	if ((input = fopen(name, "rb")) == NULL) {
		free(name);
		return NULL;
	}
	free(name);

	ENSURE(check_magic_and_version(input));
	return input;

return_error:
	if (input != NULL)
		fclose(input);
	return NULL;
}

t3_key_node_t *t3_key_load_map(const char *term, const char *map_name, int *error) {
	char *current_map = NULL, *best_map = NULL;
	t3_key_node_t *list = NULL, **next_node = &list;
	int this_map = 0;
	FILE *input;
	uint16_t node;

	if ((input = open_database(term, error)) == NULL) {
		if (errno != ENOENT)
			RETURN_ERROR(T3_ERR_ERRNO);
		return load_ti_keys(term, error);
	}

	while (fread(&node, 2, 1, input) == 1) {
		switch (ntohs(node)) {
			case NODE_BEST:
				if (current_map != NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_INVALID_FORMAT);

				if (best_map != NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_INVALID_FORMAT);

				ENSURE(read_string(input, &best_map, NULL));
				break;
			case NODE_MAP_START:
				if (best_map == NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_INVALID_FORMAT);

				if (this_map) {
					CLEANUP();
					return list;
				}

				if (current_map != NULL)
					free(current_map);
				ENSURE(read_string(input, &current_map, NULL));

				this_map = (map_name != NULL && strcmp(current_map, map_name) == 0) ||
						(map_name == NULL && strcmp(current_map, best_map) == 0);
				break;
			case NODE_KEY_VALUE:
				if (current_map == NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_INVALID_FORMAT);

				if (!this_map) {
					ENSURE(skip_string(input));
					ENSURE(skip_string(input));
					break;
				}

				ENSURE(NEW_NODE(next_node));
				ENSURE(read_string(input, &(*next_node)->key, NULL));
				ENSURE(read_string(input, &(*next_node)->string, &(*next_node)->string_length));
				next_node = &(*next_node)->next;
				break;
			case NODE_KEY_TERMINFO: {
				char *tikey, *tiresult;

				if (current_map == NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_INVALID_FORMAT);

				if (!this_map) {
					ENSURE(skip_string(input));
					ENSURE(skip_string(input));
					break;
				}

				ENSURE(NEW_NODE(next_node));
				ENSURE(read_string(input, &(*next_node)->key, NULL));
				ENSURE(read_string(input, &tikey, NULL));

				tiresult = tigetstr(tikey);
				if (tiresult == (char *)-1 || tiresult == (char *)0) {
					free(tikey);
					/* only abort when the key is %enter or %leave */
					if ((*next_node)->key[0] == '%')
						CLEANUP_RETURN_ERROR(T3_ERR_TERMINFO_UNKNOWN);
					free((*next_node)->key);
					free(*next_node);
					*next_node = NULL;
					continue;
				}
				free(tikey);
				(*next_node)->string = strdup_impl(tiresult);
				(*next_node)->string_length = strlen(tiresult);
				if ((*next_node)->string == NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);

				next_node = &(*next_node)->next;
				break;
			}
			case NODE_NAME:
			case NODE_AKA:
				ENSURE(skip_string(input));
				break;
			case NODE_SHIFTFN:
				ENSURE(NEW_NODE(next_node));
				if (((*next_node)->key = strdup_impl("%shiftfn")) == NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);
				(*next_node)->string_length = 3;
				if (((*next_node)->string = malloc(3)) == NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);
				if (fread((*next_node)->string, 1, 3, input) != 3)
					CLEANUP_RETURN_ERROR(T3_ERR_READ_ERROR);
				next_node = &(*next_node)->next;
				break;
 			case NODE_END_OF_FILE:
				if (list == NULL && error != NULL)
					*error = T3_ERR_NOMAP;

				CLEANUP();
				return list;
			default:
				CLEANUP_RETURN_ERROR(T3_ERR_INVALID_FORMAT);
		}
	}

	if (error != NULL)
		*error = EOF_OR_ERROR(input);

return_error:
	t3_key_free_map(list);
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

static int check_magic_and_version(FILE *input) {
	char magic[4];
	uint32_t version;

	if (fread(magic, 1, 4, input) != 4)
		return EOF_OR_ERROR(input);

	if (memcmp(magic, "T3KY", 4) != 0)
		return T3_ERR_INVALID_FORMAT;

	if (fread(&version, 4, 1, input) != 1)
		return EOF_OR_ERROR(input);

	if (ntohl(version) > MAX_VERSION)
		return T3_ERR_WRONG_VERSION;

	return T3_ERR_SUCCESS;
}

static int do_seek(FILE *input, uint16_t length) {
	char discard_buffer[256];

	/* Don't use fseek, because it will call lseek internally, which is not
	   necessary and causes a context switch. Because seeking will be a frequent
	   operation, this should be avoided. Furthermore, it is very likely that the
	   whole file is already buffered anyway, as it is very small.

	   As most strings will be shorter than 256 bytes anyway, we
	   don't use a 65536 bytes buffer. Using such a large buffer on the stack
	   is not desirable. */
	while (length > 0) {
		uint16_t read_now = length > 256 ? 256 : length;
		if (fread(discard_buffer, 1, read_now, input) != read_now)
			return EOF_OR_ERROR(input);
		length -= read_now;
	}
	return T3_ERR_SUCCESS;
}

static int skip_string(FILE *input) {
	uint16_t length;

	if (fread(&length, 2, 1, input) != 1)
		return EOF_OR_ERROR(input);

	return do_seek(input, ntohs(length));
}

static int read_string(FILE *input, char **string, size_t *length_ptr) {
	uint16_t length;

	if (fread(&length, 2, 1, input) != 1)
		return EOF_OR_ERROR(input);

	length = ntohs(length);
	if (length_ptr != NULL)
		*length_ptr = length;

	if ((*string = malloc(length + 1)) == NULL)
		return T3_ERR_OUT_OF_MEMORY;

	if (fread(*string, 1, length, input) != length)
		return T3_ERR_READ_ERROR;

	(*string)[length] = 0;
	return T3_ERR_SUCCESS;
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

	if (((*next_node)->key = strdup_impl(key)) == NULL) {
		free(*next_node);
		*next_node = NULL;
		return T3_ERR_OUT_OF_MEMORY;
	}

	if (((*next_node)->string = strdup_impl(tiresult)) == NULL) {
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
} Mapping;

static const Mapping keymapping[] = {
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

	{ "kIC", "insert+s" },
	{ "kDC", "delete+s" },
	{ "kHOM", "home+s" },
	{ "kEND", "end+s" },
	{ "kPRV", "page_up+s" },
	{ "kNXT", "page_down+s" },
	{ "kLFT", "left+s" },
	{ "kRIT", "right+s" },
	{ "kcbt", "tab+s" },
	{ "kent", "enter" },
	{ "kind", "down+s" },
	{ "kri", "up+s" }
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

	ENSURE(make_node_from_ti(next_node, "smkx", "%enter"));
	if (*next_node != NULL)
		next_node = &(*next_node)->next;
	ENSURE(make_node_from_ti(next_node, "rmkx", "%leave"));
	if (*next_node != NULL)
		next_node = &(*next_node)->next;

	for (i = 0; i < sizeof(keymapping)/sizeof(keymapping[0]); i++) {
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
	t3_key_string_list_t *list = NULL, **next_node = &list;
	FILE *input;
	uint16_t node;

	if ((input = open_database(term, error)) == NULL)
		return NULL;

	while (fread(&node, 2, 1, input) == 1) {
		switch (ntohs(node)) {
			case NODE_BEST:
				ENSURE(skip_string(input));
				break;
			case NODE_MAP_START:
				ENSURE(NEW_NODE(next_node));
				ENSURE(read_string(input, &(*next_node)->string, NULL));
				next_node = &(*next_node)->next;
				break;
			case NODE_KEY_VALUE:
			case NODE_KEY_TERMINFO:
				ENSURE(skip_string(input));
				ENSURE(skip_string(input));
				break;
			case NODE_NAME:
			case NODE_AKA:
				ENSURE(skip_string(input));
				break;
 			case NODE_END_OF_FILE:
				return list;
			default:
				RETURN_ERROR(T3_ERR_INVALID_FORMAT);
		}
	}

	if (error != NULL)
		*error = EOF_OR_ERROR(input);

return_error:
	t3_key_free_names(list);
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
	FILE *input;
	char *best_map;
	uint16_t node;

	if ((input = open_database(term, error)) == NULL)
		return NULL;

	if (fread(&node, 2, 1, input) != 1)
		RETURN_ERROR(T3_ERR_READ_ERROR);
	else if (node != NODE_BEST)
		RETURN_ERROR(T3_ERR_INVALID_FORMAT);

	ENSURE(read_string(input, &best_map, NULL));
	return best_map;

return_error:
	fclose(input);
	return NULL;
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

int t3_key_get_version(void) {
	return T3_KEY_VERSION;
}

const char *t3_key_strerror(int error) {
	switch (error) {
		default:
			return t3_key_strerror_base(error);

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
