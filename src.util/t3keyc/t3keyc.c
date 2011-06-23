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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <term.h>

#include "optionMacros.h"
#include "grammar.h"
#include "t3keyc.h"
#include "shareddefs.h"

#include "mappings.c"

static char *smkx;

static FILE *output;
static char *output_filename, *database_dir;
static bool error_seen;
const char *input;
extern FILE *yyin;
t3_key_map_t *maps;
char *best;
t3_key_string_list_t *akas;

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

/** Alert the user of an error.
    @param fmt The format string for the message. See fprintf(3) for details.
    @param ... The arguments for printing.
*/
void error(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	error_seen = true;
}

/** Duplicate a string but exit on allocation failure */
char *safe_strdup(const char *str) {
	char *result = malloc(strlen(str) + 1);
	if (result == NULL)
		fatal("Out of memory\n");
	return strcpy(result, str);
}

/* Print usage message/help. */
static void print_usage(void) {
	printf("Usage: t3keyc [<OPTIONS>] [<INPUT>]\n"
		"  -o<name>, --output=<name>        Name for the output file (no directories)\n"
		"  -d<dir>, --database-dir=<dir>    Directory where the database should be\n"
		"                                     written to\n"
		"  -h, --help                       Print this help message\n"
	    "\nIf no input file is given, standard input is used.\n");
	exit(EXIT_SUCCESS);
}

/* Parse command line options */
static PARSE_FUNCTION(parse_options)
	OPTIONS
		OPTION('o', "output", REQUIRED_ARG)
			if (output_filename != NULL)
				fatal("Multiple output options specified\n");
			if (strchr(optArg, '/') != NULL)
				fatal("Output name may not contain directory separators\n");
			output_filename = optArg;
		END_OPTION
		OPTION('d', "database-dir", REQUIRED_ARG)
			if (database_dir != NULL)
				fatal("Multiple database-dir options specified\n");
			database_dir = optArg;
		END_OPTION
		OPTION('h', "help", NO_ARG)
			print_usage();
		END_OPTION
		DOUBLE_DASH
			NO_MORE_OPTIONS;
		END_OPTION
		fatal("Unknown option " OPTFMT "\n", OPTPRARG);
	NO_OPTION
		if (input != NULL)
			fatal("Multiple input files specified\n");
		input = optcurrent;
	END_OPTIONS

	if (input == NULL && output_filename == NULL)
		fatal("Need output option when reading input from standard input\n");
	else if (output_filename == NULL) {
		output_filename = safe_strdup(input);
		if (strrchr(output_filename, '/') != NULL)
			output_filename = strrchr(output_filename, '/') + 1;
	}

	if (database_dir == NULL)
		database_dir = safe_strdup(".");
END_FUNCTION

/* Allocate a new map. */
t3_key_map_t *new_map(const char *name) {
	t3_key_map_t *result = calloc(1, sizeof(t3_key_map_t));

	if (result == NULL)
		fatal("Out of memory\n");

	result->line_number = line_number;
	result->name = safe_strdup(name);
	return result;
}

/** Convert a string from the input format to an internally usable string.
	@param string A @a Token with the string to be converted.
	@return The length of the resulting string.

	The use of this function processes escape characters. The converted
	characters are written in the original string.
*/
size_t parse_escapes(char *string) {
	size_t max_read_position = strlen(string) - 1;
	size_t read_position = 1, write_position = 0;
	size_t i;

	while(read_position < max_read_position) {
		if (string[read_position] == '\\') {
			read_position++;

			if (read_position == max_read_position) {
				fatal("%s:%d: Single backslash at end of string\n", input, line_number);
			}
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
							isxdigit(string[read_position + i]); i++)
					{
						value <<= 4;
						if (isdigit(string[read_position + i]))
							value += (int) (string[read_position + i] - '0');
						else
							value += (int) (tolower(string[read_position + i]) - 'a') + 10;
						if (value > UCHAR_MAX)
							/* TRANSLATORS:
							   The %s argument is a long option name without preceding dashes. */
							fatal("%s:%d: Invalid hexadecimal escape sequence in string\n", input, line_number);
					}
					read_position += i;

					if (i == 0)
						/* TRANSLATORS:
						   The %s argument is a long option name without preceding dashes. */
						fatal("%s:%d: Invalid hexadecimal escape sequence in string\n", input, line_number);

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
					string[write_position++] = string[read_position];
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

/* Reset flags that prevent multiple inclusion. */
static void clear_flags(int flags) {
	t3_key_map_t *map_ptr;
	for (map_ptr = maps; map_ptr != NULL; map_ptr = map_ptr->next)
		map_ptr->flags &= ~flags;
}

/* Allocate a new node and initialize it. Strings passed in the string argument are fully expanded. */
t3_key_node_t *new_node(const char *key, const char *string, const char *ident) {
	t3_key_node_t *result = calloc(1, sizeof(t3_key_node_t));

	if (result == NULL)
		fatal("Out of memory\n");

	result->line_number = line_number;
	result->key = key;
	if (string != NULL) {
		result->string = safe_strdup(string);
		result->string_len = parse_escapes(result->string);
	}

	if (ident != NULL)
		result->ident = safe_strdup(ident);
	result->check_ti = true;
	return result;
}

/* Find a map used as argument in a %include directive. */
static t3_key_map_t *lookup_map(const char *name) {
	t3_key_map_t *map_ptr;

	for (map_ptr = maps; map_ptr != NULL; map_ptr = map_ptr->next) {
		if (strcmp(map_ptr->name, name) == 0)
			return map_ptr;
	}
	return NULL;
}

/* Find a node by name in a map. */
static t3_key_node_t *lookup_node(t3_key_map_t *map, t3_key_node_t *needle, t3_key_map_t **other_map_store) {
	t3_key_node_t *node_ptr, *result;
	t3_key_map_t *other_map;

	if (map->flags & FLAG_MARK_LOOKUP)
		return NULL;
	map->flags |= FLAG_MARK_LOOKUP;

	for (node_ptr = map->mapping; node_ptr != NULL; node_ptr = node_ptr->next) {
		if (strcmp(node_ptr->key, "%include") == 0 && (other_map = lookup_map(node_ptr->ident)) != NULL &&
				(result = lookup_node(other_map, needle, other_map_store)) != NULL)
			return result;
		if (node_ptr->string != NULL && needle->string_len == node_ptr->string_len &&
				memcmp(node_ptr->string, needle->string, needle->string_len) == 0)
		{
			if (other_map_store != NULL)
				*other_map_store = map;
			return node_ptr;
		}
	}
	return NULL;
}

/* Search for a known terminfo to t3_key symbol mapping. Returns the index into keymapping, or -1 if not found. */
static int get_ti_mapping(const char *name) {
	int i;
	for (i = 0; i < (int) (sizeof(keymapping) / sizeof(keymapping[0])); i++) {
		if (strcmp(name, keymapping[i].key) == 0)
			return i;
	}
	return -1;
}

static bool is_function_key(const char *name) {
	if (name[0] != 'f')
		return false;
	if (strchr(name, '+') != NULL)
		return false;
	return name[1] >= '0' && name[1] <= '9';
}

/* Check the contents of a map for duplicates and non-existent %include's */
static void check_nodes(t3_key_map_t *start_map, t3_key_map_t *map, bool check_ti) {
	t3_key_node_t *node_ptr, *other_node;
	t3_key_map_t *other_map;
	int idx;

	/* Prevent loops in inclusion and double includes. */
	if (map->flags & FLAG_MARK_INCLUDED)
		return;

	map->flags |= FLAG_MARK_INCLUDED;

	for (node_ptr = map->mapping; node_ptr != NULL; node_ptr = node_ptr->next) {
		/* Check whether an %include'd map actually exists. */
		if (strcmp("%include", node_ptr->key) == 0) {
			t3_key_map_t *other_map;
			if ((other_map = lookup_map(node_ptr->ident)) == NULL)
				error("%s:%d: %%include map %s not found\n", input, node_ptr->line_number, node_ptr->ident);
			else
				check_nodes(start_map, other_map, check_ti);
			continue;
		}

		if (node_ptr->key[0] != '%' && node_ptr->string == NULL)
			continue;

		/* Check whether the current key sequence is already defined in this map.
		   Note that because we check whether the found definition for "key"
		   is the same as the current node, we automatically only emit an
		   error on second or later occurence. */
		if (node_ptr->key[0] != '%' && (other_node = lookup_node(start_map, node_ptr, &other_map)) != node_ptr)
			error("%s:%d: checking map %s: node %s:%s uses the same sequence as %s:%s on line %d\n", input, node_ptr->line_number,
				start_map->name, map->name, node_ptr->key, other_map->name, other_node->key, other_node->line_number);
		clear_flags(FLAG_MARK_LOOKUP);

		/* Check whether the key is contained in the terminfo database, and if so,
		   check whether the definition is the same. If not, emit a warning. */
		if (check_ti && node_ptr->check_ti && node_ptr->key[0] != '%') {
			char terminfo_key[100];
			char *tistr = NULL;

			if ((idx = get_ti_mapping(node_ptr->key)) >= 0) {
				strcpy(terminfo_key, keymapping[idx].tikey);
				tistr = tigetstr(terminfo_key);
			} else if (is_function_key(node_ptr->key)) {
				terminfo_key[0] = 'k';
				strcpy(terminfo_key + 1, node_ptr->key);
				tistr = tigetstr(terminfo_key);
			}

			if (!(tistr == (char *) -1 || tistr == NULL)) {
				if (strcmp(tistr, node_ptr->string) != 0)
					fprintf(stderr, "%s:%d: warning: key %s:%s has a different definition than "
						"retrieved from the terminfo database (key %s)\n",
						input, node_ptr->line_number, map->name, node_ptr->key, terminfo_key);
			}
		}
	}
}

/* Check all maps for validity. */
static void check_maps(void) {
	t3_key_map_t *map_ptr, *other_map;
	bool check_ti;

	for (map_ptr = maps; map_ptr != NULL; map_ptr = map_ptr->next) {
		if (map_ptr->name[0] == '_')
			continue;

		/* If smkx is defined, we should check whether the keys defined in the map
		   that defines %enter as smkx correspond to the definitions in the terminfo
		   database. */
		check_ti = false;
		if (smkx != NULL) {
			t3_key_node_t *node_ptr;
			for (node_ptr = map_ptr->mapping; node_ptr != NULL; node_ptr = node_ptr->next) {
				if (strcmp("%enter", node_ptr->key) == 0) {
					if ((node_ptr->ident != NULL && strcmp(node_ptr->ident, "smkx") == 0) ||
							(node_ptr->string != NULL && strcmp(node_ptr->string, smkx) == 0))
						check_ti = true;

					break;
				}
			}
		}

		if ((other_map = lookup_map(map_ptr->name)) != map_ptr)
			error("%s:%d: map %s already defined on line %d\n", input, map_ptr->line_number, map_ptr->name, other_map->line_number);

		check_nodes(map_ptr, map_ptr, check_ti);

		/* Reset all the included marks. */
		clear_flags(FLAG_MARK_INCLUDED);
	}

	if (best == NULL)
		error("%s: No %%best key was found in the file\n", input);
}

/* Write a string to the output file. This requires writing the length in network byte order,
   followed by the string contents (without the trailing zero byte). */
static void fwrite_nstring(const char *string, size_t len, FILE *output) {
	uint16_t out_short;
	out_short = htons((uint16_t) len);
	fwrite(&out_short, 1, 2, output);
	fwrite(string, 1, len, output);
}

static void fwrite_string(const char *string, FILE *output) {
	fwrite_nstring(string, strlen(string), output);
}

/* Write all nodes in a list (map). If all all_keys is false, keys starting with % will
   not be written to file. */
static void write_map(t3_key_map_t *map, bool all_keys) {
	t3_key_node_t *node_ptr;
	uint16_t tag;
	size_t string_len;
	char *string;

	if (map->flags & FLAG_MARK_INCLUDED)
		return;
	map->flags |= FLAG_MARK_INCLUDED;

	for (node_ptr = map->mapping; node_ptr != NULL; node_ptr = node_ptr->next) {
		if (strcmp("%include", node_ptr->key) == 0)
			write_map(lookup_map(node_ptr->ident), false);
		else if (node_ptr->key[0] != '%' || all_keys) {
			if (node_ptr->string != NULL) {
				tag = htons(NODE_KEY_VALUE);
				string = node_ptr->string;
				string_len = node_ptr->string_len;
			} else {
				tag = htons(NODE_KEY_TERMINFO);
				string = node_ptr->ident;
				string_len = strlen(node_ptr->ident);
			}
			fwrite(&tag, 1, 2, output);
			fwrite_string(node_ptr->key, output);
			fwrite_nstring(string, string_len, output);
		}
	}
}

/* Write the output file containing all maps. */
static void write_maps(void) {
	t3_key_map_t *map_ptr;
	const char magic[] = "T3KY";
	const char version[] = { 0, 0, 0, 0 };
	uint16_t out_short;
	char *outname;
	t3_key_string_list_t *ptr;

	if ((outname = malloc(strlen(output_filename) + strlen(database_dir) + 3 + 1)) == NULL)
		fatal("Out of memory\n");

	strcpy(outname, database_dir);
	if (!(mkdir(outname, 0777) == 0 || errno == EEXIST))
		fatal("Could not create directory %s: %s\n", outname, strerror(errno));
	strcat(outname, "/");
	strncat(outname, output_filename, 1);
	if (!(mkdir(outname, 0777) == 0 || errno == EEXIST))
		fatal("Could not create directory %s: %s\n", outname, strerror(errno));
	strcat(outname, "/");
	strcat(outname, output_filename);

	if ((output = fopen(outname, "wb")) == NULL)
		fatal("Can't open output file %s: %s\n", outname, strerror(errno));

	fwrite(magic, 1, 4, output);
	fwrite(version, 1, 4, output);

	/* First key should be %best key */
	out_short = htons(NODE_BEST);
	fwrite(&out_short, 1, 2, output);

	out_short = htons(strlen(best));
	fwrite(&out_short, 1, 2, output);
	fwrite(best, 1, strlen(best), output);

	for (map_ptr = maps; map_ptr != NULL; map_ptr = map_ptr->next) {
		if (map_ptr->name[0] == '_')
			continue;

		out_short = htons(NODE_MAP_START);
		fwrite(&out_short, 1, 2, output);
		fwrite_string(map_ptr->name, output);
		write_map(map_ptr, true);
		clear_flags(FLAG_MARK_INCLUDED);
	}

	out_short = htons(NODE_NAME);
	fwrite(&out_short, 1, 2, output);
	fwrite_string(output_filename, output);

	out_short = htons(NODE_AKA);
	ptr = akas;
	while (ptr != NULL) {
		fwrite(&out_short, 1, 2, output);
		fwrite_string(ptr->string, output);
		ptr = ptr->next;
	}

	out_short = htons(NODE_END_OF_FILE);
	fwrite(&out_short, 1, 2, output);

	if (ferror(output)) {
		fprintf(stderr, "Error writing output: %s\n", strerror(errno));
		remove(outname);
		exit(EXIT_FAILURE);
	}
	fclose(output);
	free(outname);
}

/* Create all symbolic links indicated by %aka directives. */
static void create_links(void) {
	size_t outlen;
	char *outname, *linkname, *linkcontents;
	t3_key_string_list_t *ptr;

	outlen = strlen(output_filename) + 2 + 3 + 1;
	if ((outname = malloc(outlen)) == NULL)
		fatal("Out of memory\n");
	if ((linkcontents = malloc(outlen + 2)) == NULL)
		fatal("Out of memory\n");

	strcpy(outname, "..");
	strcat(outname, "/");
	strncat(outname, output_filename, 1);
	strcat(outname, "/");
	strcat(outname, output_filename);

	ptr = akas;
	while (ptr != NULL) {
		if ((linkname = malloc(strlen(ptr->string) + strlen(database_dir) + 3 + 1)) == NULL)
			fatal("Out of memory\n");

		strcpy(linkname, database_dir);
		if (!(mkdir(linkname, 0777) == 0 || errno == EEXIST))
			fatal("Could not create directory %s: %s\n", linkname, strerror(errno));
		strcat(linkname, "/");
		strncat(linkname, ptr->string, 1);
		if (!(mkdir(linkname, 0777) == 0 || errno == EEXIST))
			fatal("Could not create directory %s: %s\n", linkname, strerror(errno));
		strcat(linkname, "/");
		strcat(linkname, ptr->string);

		if (symlink(outname, linkname) != 0) {
			if (errno == EEXIST) {
				ssize_t result;
				if ((result = readlink(linkname, linkcontents, outlen + 1)) == -1) {
					fprintf(stderr, "Could not create a link with name %s for %s "
						"because another object is in the way\n",
						linkname, output_filename);
				} else {
					linkcontents[result] = 0;
					if (strcmp(linkcontents, outname) != 0)
						fprintf(stderr, "Link with name %s already exists but does not point to %s\n",
							linkname, outname);
				}
			} else {
				fprintf(stderr, "Could not create a link with name %s for %s: %s\n",
					linkname, output_filename, strerror(errno));
			}
		}

		free(linkname);
		ptr = ptr->next;
	}
	free(outname);
}

int main(int argc, char *argv[]) {
	int errret;

	parse_options(argc, argv);

	if (input == NULL) {
		yyin = stdin;
		input = "<stdin>";
	} else if ((yyin = fopen(input, "r")) == NULL) {
		fatal("Could not open input %s: %s\n", input, strerror(errno));
	}
	parse();

	setupterm(output_filename, 1, &errret);
	smkx = tigetstr("smkx");
	if (smkx == (char *) -1)
		smkx = NULL;

	check_maps();
	if (error_seen)
		exit(EXIT_FAILURE);

	write_maps();
	if (akas != NULL)
		create_links();

	return EXIT_SUCCESS;
}
