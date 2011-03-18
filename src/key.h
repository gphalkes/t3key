/* Copyright (C) 2010 G.P. Halkes
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
#ifndef T3_KEY_H
#define T3_KEY_H

/** @defgroup t3key_other Functions, constants and enums. */
/** @addtogroup t3key_other */
/** @{ */

#include <stdlib.h>
#include "key_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The version of libt3key encoded as a single integer.

    The least significant 8 bits represent the patch level.
    The second 8 bits represent the minor version.
    The third 8 bits represent the major version.

	At runtime, the value of T3_KEY_VERSION can be retrieved by calling
	::t3_key_get_version.

    @internal
    The value 0 is an invalid value which should be replaced by the script
    that builds the release package.
*/
#define T3_KEY_VERSION 0

/* To allow modification of the structures within the key library, it defines
   the macro T3_KEY_CONST as emtpy. However, for external use the structures
   should be regarded as constant. This will prevent inadvertent modification
   of these structures.
*/
#ifndef T3_KEY_CONST
#define T3_KEY_CONST const
#endif

typedef struct t3_key_node_t t3_key_node_t;

/** A structure which is part of a singly linked list and contains a single key definition. */
struct t3_key_node_t {
	T3_KEY_CONST char *key; /**< The name of the key (with modifiers). */
	T3_KEY_CONST char *string; /**< The character sequence associated with the key. */
	T3_KEY_CONST size_t string_length; /**< The length in bytes of t3_key_node_t::string. */
	T3_KEY_CONST t3_key_node_t *next; /**< Pointer to the next ::t3_key_node_t in the singly-linked list. */
};

typedef struct t3_key_string_list_t t3_key_string_list_t;

/** A structure which is part of a singly linked list and contains a single string. */
struct t3_key_string_list_t {
	T3_KEY_CONST char *string; /**< A string. */
	T3_KEY_CONST t3_key_string_list_t *next; /**< Pointer to the next ::t3_key_string_list_t in the singly-linked list. */
};

#include "key_errors.h"

/** @name Error codes (libt3key specific) */
/*@{*/
/** Error code: invalid key-database file format. */
#define T3_ERR_INVALID_FORMAT (-32)
/** Error code: Required terminfo key not found in terminfo database. */
#define T3_ERR_TERMINFO_UNKNOWN (-31)
/** Error code: Key database contains no maps. */
#define T3_ERR_NOMAP (-30)
/** Error code: Key database is truncated. */
#define T3_ERR_TRUNCATED_DB (-29)
/** Error code: Error reading key database. */
#define T3_ERR_READ_ERROR (-28)
/** Error code: Key database is of an unsupported version. */
#define T3_ERR_WRONG_VERSION (-27)
/*@}*/

/** Load a key map from database.
    @param term The terminal name to use to find the key database.
    @param map_name Name of the map to load for the terminal.
    @param error Location to store the error code.
    @return NULL on failure, a list of ::t3_key_node_t structures on success.

    If @p term is @c NULL, the environment variable @c TERM is used to retrieve the
    terminal name. The @p map_name parameter indicates which map to load. If
    @p map_name is @c NULL, the map indicated by %best in the database is used.

    Before calling this function, you must ensure that the terminfo database
    has been initialised by calling one of @c setupterm, @c initscr, @c newterm,
    @c setterm, or the @c t3_term_init function.
*/
T3_KEY_API T3_KEY_CONST t3_key_node_t *t3_key_load_map(const char *term, const char *map_name, int *error);

/** Free a key map.
    @param list The list of keys to free.
*/
T3_KEY_API void t3_key_free_map(T3_KEY_CONST t3_key_node_t *list);

/** Get map names from database.
    @param term The terminal name to use to find the key database.
    @param error Location to store the error code.
    @return NULL on failure, a list of ::t3_key_string_list_t structures on success.

    If @p term is @c NULL, the environment variable TERM is used to retrieve the
    terminal name.
*/
T3_KEY_API T3_KEY_CONST t3_key_string_list_t *t3_key_get_map_names(const char *term, int *error);

/** Free a map names list.
    @param list The list of map names to free.
*/
T3_KEY_API void t3_key_free_names(T3_KEY_CONST t3_key_string_list_t *list);

/** Get name of best map from database.
    @param term The terminal name to use to find the key database.
    @param error Location to store the error code.
    @return NULL on failure, the name of the best map on success.

    If @p term is @c NULL, the environment variable TERM is used to retrieve the
    terminal name. The name is allocated using @c malloc.
*/
T3_KEY_API char *t3_key_get_best_map_name(const char *term, int *error);

/** Get a named node from a map.
	@param map The map to search.
	@param name The name of the node to search for, or @c NULL to continue the last search.
	@return The ::t3_key_node_t with the given name, or @c NULL if no such node exists.

    Multiple nodes may exist with the same name. To retrieve all of them, ::t3_key_get_named_node
    may be called multiple times. The second and later calls @b must use the returned value
	as the @p map parameter, and pass @c NULL as @p name.
*/
T3_KEY_API T3_KEY_CONST t3_key_node_t *t3_key_get_named_node(T3_KEY_CONST t3_key_node_t *map, const char *name);

/** Get the value of ::T3_KEY_VERSION corresponding to the actual used library.
    @ingroup t3window_other
    @return The value of ::T3_KEY_VERSION.

    This function can be useful to determine at runtime what version of the library
    was linked to the program. Although currently there are no known uses for this
    information, future library additions may prompt library users to want to operate
    differently depending on the available features.
*/
T3_KEY_API int t3_key_get_version(void);

/** Get a string description for an error code.
    @param error The error code returned by a function in libt3key.
    @return An internationalized string description for the error code.
*/
T3_KEY_API const char *t3_key_strerror(int error);

#ifdef __cplusplus
} /* extern "C" */
#endif
/** @} */
#endif
