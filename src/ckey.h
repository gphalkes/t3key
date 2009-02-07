#ifndef CKEY_H
#define CKEY_H

/* To allow modification of the structures within the ckey library, it defines
   the macro CKEY_CONST as emtpy. However, for external use the structures
   should be regarded as constant. */
#ifndef CKEY_CONST
#define CKEY_CONST const
#endif

typedef struct CKeyNode CKeyNode;

struct CKeyNode {
	CKEY_CONST char *key;
	CKEY_CONST char *string;
	CKEY_CONST CKeyNode *next;
};

typedef enum {
	CKEY_ERR_NOTERM,
	CKEY_ERR_OUTOFMEMORY,
	CKEY_ERR_OPENFAIL,
	CKEY_ERR_INVALIDFORMAT,
	CKEY_ERR_TIUNKNOWN,
	CKEY_ERR_NOMAP,
	CKEY_ERR_TRUNCATED,
	CKEY_ERR_READERROR,
	CKEY_ERR_GARBLED,
	CKEY_ERR_WRONGVERSION,
	CKEY_ERR_SUCCESS
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
CKEY_CONST CKeyNode *ckey_load(const char *term, const char *map_name, CKeyError *error);

/** Free a key database.
    @param list The list of keys to free.
*/
void ckey_free(CKEY_CONST CKeyNode *list);
#endif
