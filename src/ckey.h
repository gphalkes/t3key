#ifndef CKEY_H
#define CKEY_H

#ifdef __cplusplus
extern "C" {
#endif

/* API version implemented by this version of the library. The major version
   changes when incompatible changes are made. The minor version will increase
   when compatible extensions of the API are added.
*/
#define CKEY_API_MAJOR_VERSION 0
#define CKEY_API_MINOR_VERSION 0
#define CKEY_API_VERSION (CKEY_API_MAJOR_VERSION << 16 | CKEY_API_MINOR_VERSION)
#define CKEY_API_MAJOR_VERSION_FROM_FULL(_x) ((_x) >> 16)
#define CKEY_API_MINOR_VERSION_FROM_FULL(_x) ((_x) & 0xFFFF)

/* To allow modification of the structures within the ckey library, it defines
   the macro CKEY_CONST as emtpy. However, for external use the structures
   should be regarded as constant. This will prevent inadvertent modification
   of these structures.
*/
#ifndef CKEY_CONST
#define CKEY_CONST const
#endif

typedef struct CKeyNode CKeyNode;
struct CKeyNode {
	CKEY_CONST char *key;
	CKEY_CONST char *string;
	CKEY_CONST CKeyNode *next;
};

typedef struct CKeyStringListNode CKeyStringListNode;
struct CKeyStringListNode {
	CKEY_CONST char *string;
	CKEY_CONST CKeyStringListNode *next;
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

/** Load a key map from database.
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
CKEY_CONST CKeyNode *ckey_load_map(const char *term, const char *map_name, CKeyError *error);

/** Free a key map.
    @param list The list of keys to free.
*/
void ckey_free_map(CKEY_CONST CKeyNode *list);

/** Get map names from database.
    @param term The terminal name to use to find the key database.
    @return NULL on failure, a list of @a CKeyStringListNode structures on success.

    If @a term is @a NULL, the environment variable TERM is used to retrieve the
    terminal name.
*/
CKEY_CONST CKeyStringListNode *ckey_get_map_names(const char *term);

/** Free a map names list.
    @param list The list of map names to free.
*/
void ckey_free_names(CKEY_CONST CKeyStringListNode *list);

/** Get name of best map from database.
    @param term The terminal name to use to find the key database.
    @return NULL on failure, the name of the best map on success.

    If @a term is @a NULL, the environment variable TERM is used to retrieve the
    terminal name. The name is allocated using @a malloc.
*/
char *ckey_get_best_map_name(const char *term);

/** Get a named node from a map.
	@param map The map to search.
	@param name The name of the node to search for.
	@return The @a CKeyNode with the given name, or NULL if no such node exists.
*/
CKEY_CONST CKeyNode *ckey_get_named_node(CKEY_CONST CKeyNode *map, const char *name);

/** Get API version
	@return The API version as defined by CKEY_API_VERSION.

	This routine exists such that when using dlopen, programs can still verify
	that the library implements the expected API version. For testing at compile
	time, the CKEY_API_VERSION macro should be tested.
*/
int ckey_get_api_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
