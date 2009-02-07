#include <stdlib.h>
#include <stdio.h>

#include "ckey.h"

enum {
	NODE_BEST,
	NODE_MAP_START,
	NODE_KEY_VALUE,
	NODE_KEY_TERMINFO,
	NODE_END_OF_FILE
};


#define RETURN_ERROR(_e) do { if (error != NULL) *error = (_e); return NULL; } while(0);
#define CLEANUP_RETURN_ERROR(_e) do { fclose(input); ckey_free(list); RETURN_ERROR(_e); } while (0);
#define ENSURE(_x) do { CKeyError _error = (_x); \
	if (_error == CKEY_ERR_SUCCESS) \
		break; \
	if (error != NULL) \
		*error = _error; \
	fclose(input); \
	ckey_free(list); \
	return NULL; } while (0);
#define EOF_OR_ERROR(_file) (feof(_file) ? CKEY_ERR_TRUNCATED : CKEY_ERR_READERROR)

static CKeyError check_magic_and_version(FILE *input);
static CKeyError skip_string(FILE *input);
static CKeyError read_string(FILE *input, char **string);

const CKeyNode *ckey_load(const char *term, const char *map_name, CKeyError *error) {
	char *current_map = NULL, *best_map = NULL;
	size_t name_length, term_length;
	CKeyNode *list = NULL, **next_node = &list;
	int this_map = 0;
	char *name;

	if (term == NULL) {
		term = getenv(term);
		if (term == NULL)
			RETURN_ERROR(CKEY_ERR_NOTERM);
	}

	term_length = strlen(term);
	name_length = strlen(DB_DIRECTORY) + 1 + term_length + 1;
	if ((name = malloc(name_length)) == NULL)
		RETURN_ERROR(CKEY_ERR_OUTOFMEMORY);

	strcpy(name, DB_DIRECTORY);
	strcat(name, "/");
	strncat(name, term, term_length);
	name[name_length - 1] = 0;

	//FIXME: fall back to terminfo if file does not exist!
	if ((input = fopen(name, "rb")) == NULL)
		RETURN_ERROR(CKEY_ERR_OPENFAIL);

	ENSURE(check_magic_and_version(input));
	while (fread(&node, 2, 1, input) == 1) {
		switch (ntohs(node)) {
			case NODE_BEST:
				if (current_map != NULL)
					CLEANUP_RETURN_ERROR(CKEY_ERR_INVALIDFORMAT);

				if (map_name == NULL)
					ENSURE(skip_string(input));
				else
					ENSURE(read_string(input, &best_map));
				break;
			case NODE_MAP_START:
				if (best_map == NULL)
					CLEANUP_RETURN_ERROR(CKEY_ERR_INVALIDFORMAT);

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
				ENSURE(read_string(input, &tikey);

				tiresult = tigetstr(tikey);
				//FIXME: only abort when the key is %enter or %leave
				if (tiresult == (char *)-1 || tiresult == (char *)0)
					CLEANUP_RETURN_ERROR(CKEY_ERR_TIUNKNOWN);
				free(tikey);
				(*next_node)->string = strdup(tiresult);
				if ((*next_node)->string == NULL)
					CLEANUP_RETURN_ERROR(CKEY_ERR_OUTOFMEMORY);

				next_node = &(*next_node)->next;
				break;
 			case NODE_END_OF_FILE:
				fclose(input);
				if (list == NULL && error != NULL)
					*error = CKEY_ERR_NOMAP;
				return list;
	}

	result = EOF_OR_ERROR(input);
	fclose(input);
	ckey_free(list);
	return NULL;
}


/** Free a key database.
    @param list The list of keys to free.
*/
void ckey_free(const CKeyNode *list) {
	const CKeyNode *prev;
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

	if (memcmp(magic, "CKEY") != 0)
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

	*string[length] = 0;
	return CKEY_ERR_SUCCESS;
}

static CKeyError new_ckey_node(CKeyNode **result) {
	if ((*result = malloc(sizeof(CKeyNode))) == NULL)
		return CKEY_ERR_OUTOFMEMORY;
	memset(*result, 0, sizeof(CKeyNode));
	return CKEY_ERR_SUCCESS;
}
