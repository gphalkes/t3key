#ifndef CKEY_H
#define CKEY_H

typedef struct CKeyNode CKeyNode;

struct CKeyNode {
	const char *key;
	const char *string;
	const CKeyNode *next;
};

typedef enum {
	CKEY_ERR_NOTERM,
	CKEY_ERR_OUTOFMEMORY,
	CKEY_ERR_OPENFAIL

} CKeyError;

/** Load a key database.
    @param term The terminal name to use to find the key database.
    @param map_name Name of the map to load for the terminal.
    @param error Location to store the error code.
    @return NULL on failure, a list of @a CKeyNode structures on success.

    If @a term is @a NULL, the environment variable TERM is used to retrieve the
    terminal name. The @a map_name parameter indicates which map to load. If
    @a map_name is @a NULL, the map indicated by %best in the database is used.

    Before calling this function, you must ensure that the terminfo database
    has been initialised by calling one of @a setupterm, @a initscr, @a newterm
    or @a setterm.
*/
const CKeyNode *ckey_load(const char *term, const char *map_name, CKeyError *error);

/** Free a key database.
    @param list The list of keys to free.
*/
void ckey_free(const CKeyNode *list);
#endif
