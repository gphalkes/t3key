#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "optionMacros.h"
#include "grammar.h"
#include "t3keyc.h"
#include "shareddefs.h"

typedef enum { false, true } bool;
static FILE *output;
static char *output_filename;

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

static bool error_seen;
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
	char *result = strdup(str);
	if (result == NULL)
		fatal("Out of memory\n");
	return result;
}

static const char *input;
extern FILE *yyin;

static PARSE_FUNCTION(parse_options)
	OPTIONS
		OPTION('o', "output", REQUIRED_ARG)
			output_filename = optArg;
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

		if (input == NULL && output_filename == NULL) {
		fatal("Need output option when reading input from standard input\n");
	} else if (output_filename == NULL) {
		output_filename = malloc(strlen(input) + 6);
		if (output_filename == NULL)
			fatal("Out of memory\n");
		strcpy(output_filename, input);
		strcat(output_filename, ".ckey");
	}
END_FUNCTION

CKeyMap *maps;
char *best;

CKeyMap *new_map(const char *name) {
	CKeyMap *result = calloc(1, sizeof(CKeyMap));

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
static size_t parse_escapes(char *string) {
	size_t maxReadPosition = strlen(string) - 1;
	size_t readPosition = 1, writePosition = 0;
	size_t i;

	while(readPosition < maxReadPosition) {
		if (string[readPosition] == '\\') {
			readPosition++;

			if (readPosition == maxReadPosition) {
				fatal("%d: Single backslash at end of string\n", line_number);
			}
			switch(string[readPosition++]) {
				case 'E':
				case 'e':
					string[writePosition++] = '\033';
					break;
				case 'n':
					string[writePosition++] = '\n';
					break;
				case 'r':
					string[writePosition++] = '\r';
					break;
				case '\'':
					string[writePosition++] = '\'';
					break;
				case '\\':
					string[writePosition++] = '\\';
					break;
				case 't':
					string[writePosition++] = '\t';
					break;
				case 'b':
					string[writePosition++] = '\b';
					break;
				case 'f':
					string[writePosition++] = '\f';
					break;
				case 'a':
					string[writePosition++] = '\a';
					break;
				case 'v':
					string[writePosition++] = '\v';
					break;
				case '?':
					string[writePosition++] = '\?';
					break;
				case '"':
					string[writePosition++] = '"';
					break;
				case 'x': {
					/* Hexadecimal escapes */
					unsigned int value = 0;
					/* Read at most two characters, or as many as are valid. */
					for (i = 0; i < 2 && (readPosition + i) < maxReadPosition && isxdigit(string[readPosition + i]); i++) {
						value <<= 4;
						if (isdigit(string[readPosition + i]))
							value += (int) (string[readPosition + i] - '0');
						else
							value += (int) (tolower(string[readPosition + i]) - 'a') + 10;
						if (value > UCHAR_MAX)
							/* TRANSLATORS:
							   The %s argument is a long option name without preceding dashes. */
							fatal("%d: Invalid hexadecimal escape sequence in string\n", line_number);
					}
					readPosition += i;

					if (i == 0)
						/* TRANSLATORS:
						   The %s argument is a long option name without preceding dashes. */
						fatal("%d: Invalid hexadecimal escape sequence in string\n", line_number);

					string[writePosition++] = (char) value;
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
					int value = (int)(string[readPosition - 1] - '0');
					size_t maxIdx = string[readPosition - 1] < '4' ? 2 : 1;
					for (i = 0; i < maxIdx && readPosition + i < maxReadPosition && string[readPosition + i] >= '0' && string[readPosition + i] <= '7'; i++)
						value = value * 8 + (int)(string[readPosition + i] - '0');

					readPosition += i;

					string[writePosition++] = (char) value;
					break;
				}
				default:
					string[writePosition++] = string[readPosition];
					break;
			}
		} else {
			string[writePosition++] = string[readPosition++];
		}
	}
	/* Terminate string. */
	string[writePosition] = 0;
	return writePosition;
}

CKeyNode *new_node(const char *key, const char *string, const char *ident) {
	CKeyNode *result = calloc(1, sizeof(CKeyNode));

	if (result == NULL)
		fatal("Out of memory\n");

	result->line_number = line_number;
	result->key = key;
	if (string != NULL) {
		result->string = safe_strdup(string);
		parse_escapes(result->string);
	}

	if (ident != NULL)
		result->ident = safe_strdup(ident);
	return result;
}

static CKeyNode *lookup_node(CKeyMap *map, const char *key) {
	CKeyNode *node_ptr;

	for (node_ptr = map->mapping; node_ptr != NULL; node_ptr = node_ptr->next) {
		if (strcmp(node_ptr->key, key) == 0)
			return node_ptr;
	}
	return NULL;
}

static CKeyMap *lookup_map(const char *name) {
	CKeyMap *map_ptr;

	for (map_ptr = maps; map_ptr != NULL; map_ptr = map_ptr->next) {
		if (strcmp(map_ptr->name, name) == 0)
			return map_ptr;
	}
	return NULL;
}

static void check_nodes(CKeyMap *map) {
	CKeyNode *node_ptr, *other_node;

	for (node_ptr = map->mapping; node_ptr != NULL; node_ptr = node_ptr->next) {
		if ((other_node = lookup_node(map, node_ptr->key)) != node_ptr)
			error("%d: node %s:%s already defined on line %d\n", node_ptr->line_number, map->name, node_ptr->key, other_node->line_number);

		if (strcmp("%include", node_ptr->key) == 0)
			if (lookup_map(node_ptr->ident) == NULL)
				error("%d: %%include map %s not found\n", node_ptr->line_number, node_ptr->string);
	}
}

static void check_maps(void) {
	CKeyMap *map_ptr, *other_map;

	for (map_ptr = maps; map_ptr != NULL; map_ptr = map_ptr->next) {
		if ((other_map = lookup_map(map_ptr->name)) != map_ptr)
			error("%d: map %s already defined on line %d\n", map_ptr->line_number, map_ptr->name, other_map->line_number);

		check_nodes(map_ptr);
	}
	if (best == NULL)
		error("No %%best key was found in the file\n");
}

static void write_nodes(CKeyNode *nodes, bool all_keys) {
	CKeyNode *node_ptr;
	uint16_t out_short;
	char *string;

	for (node_ptr = nodes; node_ptr != NULL; node_ptr = node_ptr->next) {
		if (strcmp("%include", node_ptr->key) == 0)
			write_nodes(lookup_map(node_ptr->ident)->mapping, false);
		else if (node_ptr->key[0] != '%' || all_keys) {
			if (node_ptr->string != NULL) {
				out_short = htons(NODE_KEY_VALUE);
				string = node_ptr->string;
			} else {
				out_short = htons(NODE_KEY_TERMINFO);
				string = node_ptr->ident;
			}
			fwrite(&out_short, 1, 2, output);
			out_short = htons(strlen(node_ptr->key));
			fwrite(&out_short, 1, 2, output);
			fwrite(node_ptr->key, 1, strlen(node_ptr->key), output);
			out_short = htons(strlen(string));
			fwrite(&out_short, 1, 2, output);
			fwrite(string, 1, strlen(string), output);
		}
	}
}

static void write_maps(void) {
	CKeyMap *map_ptr;
	const char magic[] = "CKEY";
	const char version[] = { 0, 0, 0, 0 };
	uint16_t out_short;

	if ((output = fopen(output_filename, "wb")) == NULL)
		fatal("Can't open output file %s: %s\n", output_filename, strerror(errno));

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
		out_short = htons(strlen(map_ptr->name));
		fwrite(&out_short, 1, 2, output);
		fwrite(map_ptr->name, 1, strlen(map_ptr->name), output);
		write_nodes(map_ptr->mapping, true);
	}
	out_short = htons(NODE_END_OF_FILE);
	fwrite(&out_short, 1, 2, output);

	if (ferror(output)) {
		fprintf(stderr, "Error writing output: %s\n", strerror(errno));
		remove(output_filename);
		exit(EXIT_FAILURE);
	}
	fclose(output);
}

int main(int argc, char *argv[]) {
	parse_options(argc, argv);

	if (input == NULL)
		yyin = stdin;
	else if ((yyin = fopen(input, "r")) == NULL)
		fatal("Could not open input %s: %s\n", input, strerror(errno));
	parse();

	check_maps();
	if (error_seen)
		exit(EXIT_FAILURE);

	write_maps();

	return EXIT_SUCCESS;
}
