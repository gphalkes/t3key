#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>
#include <errno.h>
#include <stdint.h>
#include <arpa/inet.h>

#define CKEY_CONST
#include "ckey.h"

#ifndef DB_DIRECTORY
#define DB_DIRECTORY "/usr/local/share/libckey"
#endif

#define MAX_VERSION 0

enum {
	NODE_BEST,
	NODE_MAP_START,
	NODE_KEY_VALUE,
	NODE_KEY_TERMINFO,
	NODE_END_OF_FILE = 65535
};


#define RETURN_ERROR(_e) do { if (error != NULL) *error = (_e); return NULL; } while(0)
#define CLEANUP_RETURN_ERROR(_e) do { fclose(input); ckey_free_map(list); RETURN_ERROR(_e); } while (0)
#define ENSURE(_x) do { CKeyError _error = (_x); \
	if (_error == CKEY_ERR_SUCCESS) \
		break; \
	if (error != NULL) \
		*error = _error; \
	fclose(input); \
	ckey_free_map(list); \
	return NULL; } while (0)
#define EOF_OR_ERROR(_file) (feof(_file) ? CKEY_ERR_TRUNCATED : CKEY_ERR_READERROR)

static CKeyError check_magic_and_version(FILE *input);
static CKeyError skip_string(FILE *input);
static CKeyError read_string(FILE *input, char **string);
static CKeyError new_ckey_node(CKeyNode **result);
static CKeyNode *load_ti_keys(CKeyError *error);

CKeyNode *ckey_load_map(const char *term, const char *map_name, CKeyError *error) {
	char *current_map = NULL, *best_map = NULL;
	size_t name_length, term_length;
	CKeyNode *list = NULL, **next_node = &list;
	int this_map = 0;
	char *name;
	FILE *input;
	uint16_t node;

	if (term == NULL) {
		term = getenv("TERM");
		if (term == NULL)
			RETURN_ERROR(CKEY_ERR_NOTERM);
	}

	term_length = strlen(term);
	name_length = strlen(DB_DIRECTORY) + 3 + term_length + 1;
	if ((name = malloc(name_length)) == NULL)
		RETURN_ERROR(CKEY_ERR_OUTOFMEMORY);

	strcpy(name, DB_DIRECTORY);
	strcat(name, "/");
	strncat(name, term, 1);
	strcat(name, "/");
	strncat(name, term, term_length);
	name[name_length - 1] = 0;

	if ((input = fopen(name, "rb")) == NULL) {
		if (errno != ENOENT)
			RETURN_ERROR(CKEY_ERR_OPENFAIL);
		return load_ti_keys(error);
	}

	ENSURE(check_magic_and_version(input));
	while (fread(&node, 2, 1, input) == 1) {
		switch (ntohs(node)) {
			case NODE_BEST:
				if (current_map != NULL)
					CLEANUP_RETURN_ERROR(CKEY_ERR_INVALIDFORMAT);

				ENSURE(read_string(input, &best_map));
				break;
			case NODE_MAP_START:
				if (best_map == NULL)
					CLEANUP_RETURN_ERROR(CKEY_ERR_INVALIDFORMAT);

				if (this_map) {
					fclose(input);
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
					CLEANUP_RETURN_ERROR(CKEY_ERR_INVALIDFORMAT);

				if (!this_map) {
					ENSURE(skip_string(input));
					ENSURE(skip_string(input));
					break;
				}

				ENSURE(new_ckey_node(next_node));
				ENSURE(read_string(input, &(*next_node)->key));
				ENSURE(read_string(input, &(*next_node)->string));
				next_node = &(*next_node)->next;
				break;
			case NODE_KEY_TERMINFO: {
				char *tikey, *tiresult;

				if (current_map == NULL)
					CLEANUP_RETURN_ERROR(CKEY_ERR_INVALIDFORMAT);

				if (!this_map) {
					ENSURE(skip_string(input));
					ENSURE(skip_string(input));
					break;
				}

				ENSURE(new_ckey_node(next_node));
				ENSURE(read_string(input, &(*next_node)->key));
				ENSURE(read_string(input, &tikey));

				tiresult = tigetstr(tikey);
				if (tiresult == (char *)-1 || tiresult == (char *)0) {
					/* only abort when the key is %enter or %leave */
					if ((*next_node)->key[0] == '%')
						CLEANUP_RETURN_ERROR(CKEY_ERR_TIUNKNOWN);
					free((*next_node)->key);
					free(*next_node);
					*next_node = NULL;
					continue;
				}
				free(tikey);
				(*next_node)->string = strdup(tiresult);
				if ((*next_node)->string == NULL)
					CLEANUP_RETURN_ERROR(CKEY_ERR_OUTOFMEMORY);

				next_node = &(*next_node)->next;
				break;
			}
 			case NODE_END_OF_FILE:
				fclose(input);
				if (list == NULL && error != NULL)
					*error = CKEY_ERR_NOMAP;
				return list;
			default:
				CLEANUP_RETURN_ERROR(CKEY_ERR_GARBLED);
		}
	}

	if (error != NULL)
		*error = EOF_OR_ERROR(input);
	fclose(input);
	ckey_free_map(list);
	return NULL;
}


void ckey_free_map(CKeyNode *list) {
	CKeyNode *prev;
	while (list != NULL) {
		prev = list;
		list = list->next;
		free(prev->key);
		free(prev->string);
		free(prev);
	}
}

static CKeyError check_magic_and_version(FILE *input) {
	char magic[4];
	uint32_t version;

	if (fread(magic, 1, 4, input) != 4)
		return EOF_OR_ERROR(input);

	if (memcmp(magic, "CKEY", 4) != 0)
		return CKEY_ERR_GARBLED;

	if (fread(&version, 4, 1, input) != 1)
		return EOF_OR_ERROR(input);

	if (ntohl(version) > MAX_VERSION)
		return CKEY_ERR_WRONGVERSION;

	return CKEY_ERR_SUCCESS;
}

static CKeyError skip_string(FILE *input) {
	uint16_t length;

	if (fread(&length, 2, 1, input) != 1)
		return EOF_OR_ERROR(input);

	if (fseek(input, ntohs(length), SEEK_CUR) != 0)
		return CKEY_ERR_READERROR;
	return CKEY_ERR_SUCCESS;
}

static CKeyError read_string(FILE *input, char **string) {
	uint16_t length;

	if (fread(&length, 2, 1, input) != 1)
		return EOF_OR_ERROR(input);

	length = ntohs(length);

	if ((*string = malloc(length + 1)) == NULL)
		return CKEY_ERR_OUTOFMEMORY;

	if (fread(*string, 1, length, input) != length)
		return CKEY_ERR_READERROR;

	(*string)[length] = 0;
	return CKEY_ERR_SUCCESS;
}

static CKeyError new_ckey_node(CKeyNode **result) {
	if ((*result = malloc(sizeof(CKeyNode))) == NULL)
		return CKEY_ERR_OUTOFMEMORY;
	memset(*result, 0, sizeof(CKeyNode));
	return CKEY_ERR_SUCCESS;
}

static CKeyError make_node_from_ti(CKeyNode **next_node, const char *tikey, const char *key) {
	char *tiresult;
	CKeyError error;

	tiresult = tigetstr(tikey);
	if (tiresult == (char *)0 || tiresult == (char *)-1)
		return CKEY_ERR_SUCCESS;

	if ((error = new_ckey_node(next_node)) != CKEY_ERR_SUCCESS)
		return error;

	if (((*next_node)->key = strdup(key)) == NULL) {
		free(*next_node);
		*next_node = NULL;
		return CKEY_ERR_OUTOFMEMORY;
	}

	if (((*next_node)->string = strdup(tiresult)) == NULL) {
		free((*next_node)->string);
		free(*next_node);
		*next_node = NULL;
		return CKEY_ERR_OUTOFMEMORY;
	}

	return CKEY_ERR_SUCCESS;
}

typedef struct {
	const char *tikey;
	const char *key;
} Mapping;

static const Mapping keymapping[] = {
	{ "smkx", "%enter" },
	{ "rmkx", "%leave" },
	{ "khome", "kp_home" }, //FIXME: shouldn't this be home and ka1 be kp_home??
	{ "kend", "kp_end" },
	{ "kcud1", "kp_down" },
	{ "kcuu1", "kp_up" },
	{ "kcub1", "kp_left" },
	{ "kcuf1", "kp_right" },
	{ "kpp", "kp_page_up" },
	{ "knp", "kp_page_down" },
	{ "kdch1", "kp_delete" },
	{ "kich1", "kp_insert" }
	//FIXME: add all the keys we know of
};

static CKeyNode *load_ti_keys(CKeyError *error) {
	CKeyNode *list = NULL, **next_node = &list;
	CKeyError _error;
	size_t i;
	//FIXME: check return values. Normal macro's don't work because there is no input FILE.
	//FIXME: use the terminfo database to provide the keys

	for (i = 0; i < sizeof(keymapping)/sizeof(keymapping[0]); i++) {
		if ((_error = make_node_from_ti(next_node, keymapping[i].tikey, keymapping[i].key)) != CKEY_ERR_SUCCESS) {
			ckey_free_map(list);
			if (error != NULL)
				*error = _error;
			return NULL;
		}
		if (*next_node != NULL)
			next_node = &(*next_node)->next;
	}

	return list;
}


/* FIXME: implement ckey_get_map_names */

void ckey_free_names(CKeyStringListNode *list) {
	CKeyStringListNode *prev;
	while (list != NULL) {
		prev = list;
		list = list->next;
		free(prev->string);
		free(prev);
	}
}

/* FIXME: implement ckey_get_best_map_name */

CKeyNode *ckey_get_named_node(CKEY_CONST CKeyNode *map, const char *name) {
	for (; map != NULL; map = map->next) {
		if (strcmp(map->key, name) == 0)
			return map;
	}
	return NULL;
}
