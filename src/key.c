#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>
#include <errno.h>
#include <stdint.h>
#include <arpa/inet.h>

#ifdef USE_GETTEXT
#include <libintl.h>
#define _(x) dgettext("LIBT3", (x))
#else
#define _(x) (x)
#endif

#define T3_KEY_CONST
#include "key.h"

#ifndef DB_DIRECTORY
#define DB_DIRECTORY "/usr/local/share/libckey"
#endif

#define MAX_VERSION 0

#include "shareddefs.h"

#define RETURN_ERROR(_e) do { if (error != NULL) *error = _e; goto return_error; } while (0)
#define CLEANUP() do { free(best_map); free(current_map); fclose(input); } while (0)
#define CLEANUP_RETURN_ERROR(_e) do { CLEANUP(); t3_key_free_map(list); RETURN_ERROR(_e); } while (0)
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
static int read_string(FILE *input, char **string);
static int new_node(void **result, size_t size);
#define NEW_NODE(_x) new_node((void **) (_x), sizeof(**(_x)))
static t3_key_node_t *load_ti_keys(int *error);

static FILE *open_database(const char *term, int *error) {
	size_t name_length, term_length;
	FILE *input = NULL;
	char *name;

	if (term == NULL) {
		term = getenv("TERM");
		if (term == NULL)
			RETURN_ERROR(T3_ERR_NO_TERM);
	}

	term_length = strlen(term);
	name_length = strlen(DB_DIRECTORY) + 3 + term_length + 1;
	if ((name = malloc(name_length)) == NULL)
		RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);

	strcpy(name, DB_DIRECTORY);
	strcat(name, "/");
	strncat(name, term, 1);
	strcat(name, "/");
	strncat(name, term, term_length);
	name[name_length - 1] = 0;

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
		return load_ti_keys(error);
	}

	while (fread(&node, 2, 1, input) == 1) {
		switch (ntohs(node)) {
			case NODE_BEST:
				if (current_map != NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_INVALID_FORMAT);

				if (best_map != NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_INVALID_FORMAT);

				ENSURE(read_string(input, &best_map));
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
				ENSURE(read_string(input, &current_map));

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
				ENSURE(read_string(input, &(*next_node)->key));
				ENSURE(read_string(input, &(*next_node)->string));
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
				ENSURE(read_string(input, &(*next_node)->key));
				ENSURE(read_string(input, &tikey));

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
				(*next_node)->string = strdup(tiresult);
				if ((*next_node)->string == NULL)
					CLEANUP_RETURN_ERROR(T3_ERR_OUT_OF_MEMORY);

				next_node = &(*next_node)->next;
				break;
			}
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
	CLEANUP();
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

	if (memcmp(magic, "CKEY", 4) != 0)
		return T3_ERR_INVALID_FORMAT;

	if (fread(&version, 4, 1, input) != 1)
		return EOF_OR_ERROR(input);

	if (ntohl(version) > MAX_VERSION)
		return T3_ERR_WRONG_VERSION;

	return T3_ERR_SUCCESS;
}

static int skip_string(FILE *input) {
	uint16_t length;

	if (fread(&length, 2, 1, input) != 1)
		return EOF_OR_ERROR(input);

	if (fseek(input, ntohs(length), SEEK_CUR) != 0)
		return T3_ERR_READ_ERROR;
	return T3_ERR_SUCCESS;
}

static int read_string(FILE *input, char **string) {
	uint16_t length;

	if (fread(&length, 2, 1, input) != 1)
		return EOF_OR_ERROR(input);

	length = ntohs(length);

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

	if (((*next_node)->key = strdup(key)) == NULL) {
		free(*next_node);
		*next_node = NULL;
		return T3_ERR_OUT_OF_MEMORY;
	}

	if (((*next_node)->string = strdup(tiresult)) == NULL) {
		free((*next_node)->string);
		free(*next_node);
		*next_node = NULL;
		return T3_ERR_OUT_OF_MEMORY;
	}

	return T3_ERR_SUCCESS;
}

typedef struct {
	const char *tikey;
	const char *key;
} Mapping;

static const Mapping keymapping[] = {
	{ "smkx", "%enter" },
	{ "rmkx", "%leave" },
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
	{ "kLFT", "left+s" },
	{ "kRIT", "right+s" },
	{ "kcbt", "tab+s" },
	{ "kent", "enter" },
};

static t3_key_node_t *load_ti_keys(int *error) {
	t3_key_node_t *list = NULL, **next_node = &list;
	size_t i;

	for (i = 0; i < sizeof(keymapping)/sizeof(keymapping[0]); i++) {
		ENSURE(make_node_from_ti(next_node, keymapping[i].tikey, keymapping[i].key));
		if (*next_node != NULL)
			next_node = &(*next_node)->next;
	}
	#warning FIXME: add function keys programmatically (f1-f63)
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
				ENSURE(read_string(input, &(*next_node)->string));
				next_node = &(*next_node)->next;
				break;
			case NODE_KEY_VALUE:
			case NODE_KEY_TERMINFO:
				ENSURE(skip_string(input));
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

	ENSURE(read_string(input, &best_map));
	return best_map;

return_error:
	fclose(input);
	return NULL;
}

t3_key_node_t *t3_key_get_named_node(T3_KEY_CONST t3_key_node_t *map, const char *name) {
	for (; map != NULL; map = map->next) {
		if (strcmp(map->key, name) == 0)
			return map;
	}
	return NULL;
}

int t3_key_get_version(void) {
	return T3_KEY_API_VERSION;
}

const char *t3_key_strerror(int error) {
	switch (error) {
		case T3_ERR_SUCCESS:
			return _("Success");
		case T3_ERR_ERRNO:
			return strerror(errno);
		case T3_ERR_EOF:
			return _("End of file");
		default: /* FALLTHROUGH */
		case T3_ERR_UNKNOWN:
			return _("Unknown error");
		case T3_ERR_BAD_ARG:
			return _("Bad argument passed to function");
		case T3_ERR_OUT_OF_MEMORY:
			return _("Out of memory");
		case T3_ERR_NO_TERM:
			return _("No terminal given and TERM environment variable not set");
		case T3_ERR_INVALID_FORMAT:
			return _("Invalid key-database file format");
		case T3_ERR_TERMINFO_UNKNOWN:
			return _("Required terminfo key not found in terminfo database");
		case T3_ERR_NOMAP:
			return _("Key database contains no maps");
		case T3_ERR_TRUNCATED_DB:
			return _("Key database is truncated");
		case T3_ERR_READ_ERROR:
			return _("Error reading key database");
		case T3_ERR_WRONG_VERSION:
			return _("Key database is of an unsupported version");
	}
}
