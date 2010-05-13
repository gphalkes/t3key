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

typedef enum { false, true } bool;

/* FIXME: we should only keep one copy of this (also in src/key.c) */
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
	{ "KNXT", "page_up+s" },
	{ "KPRV", "page_down+s" },
	{ "kLFT", "left+s" },
	{ "kRIT", "right+s" },
	{ "kcbt", "tab+s" },
	{ "kent", "enter" },
};

static char *smkx;

static FILE *output;
static char *output_filename, *database_dir;
static bool error_seen;
static const char *input;
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
	char *result = strdup(str);
	if (result == NULL)
		fatal("Out of memory\n");
	return result;
}

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
				fatal("%d: Single backslash at end of string\n", line_number);
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
					for (i = 0; i < 2 && (read_position + i) < max_read_position && isxdigit(string[read_position + i]); i++) {
						value <<= 4;
						if (isdigit(string[read_position + i]))
							value += (int) (string[read_position + i] - '0');
						else
							value += (int) (tolower(string[read_position + i]) - 'a') + 10;
						if (value > UCHAR_MAX)
							/* TRANSLATORS:
							   The %s argument is a long option name without preceding dashes. */
							fatal("%d: Invalid hexadecimal escape sequence in string\n", line_number);
					}
					read_position += i;

					if (i == 0)
						/* TRANSLATORS:
						   The %s argument is a long option name without preceding dashes. */
						fatal("%d: Invalid hexadecimal escape sequence in string\n", line_number);

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
					for (i = 0; i < max_idx && read_position + i < max_read_position && string[read_position + i] >= '0' && string[read_position + i] <= '7'; i++)
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

t3_key_node_t *new_node(const char *key, const char *string, const char *ident) {
	t3_key_node_t *result = calloc(1, sizeof(t3_key_node_t));

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

static t3_key_node_t *lookup_node(t3_key_map_t *map, const char *key) {
	t3_key_node_t *node_ptr;

	for (node_ptr = map->mapping; node_ptr != NULL; node_ptr = node_ptr->next) {
		if (strcmp(node_ptr->key, key) == 0)
			return node_ptr;
	}
	return NULL;
}

static t3_key_map_t *lookup_map(const char *name) {
	t3_key_map_t *map_ptr;

	for (map_ptr = maps; map_ptr != NULL; map_ptr = map_ptr->next) {
		if (strcmp(map_ptr->name, name) == 0)
			return map_ptr;
	}
	return NULL;
}

static int get_ti_mapping(const char *name) {
	int i;
	for (i = 0; i < (int) (sizeof(keymapping) / sizeof(keymapping[0])); i++) {
		if (strcmp(name, keymapping[i].key) == 0)
			return i;
	}
	return -1;
}

static void check_nodes(t3_key_map_t *map) {
	t3_key_node_t *node_ptr, *other_node;
	bool check_ti = false;
	int idx;

	if (smkx != NULL) {
		for (node_ptr = map->mapping; node_ptr != NULL; node_ptr = node_ptr->next) {
			if (strcmp("%enter", node_ptr->key) == 0) {
				if (node_ptr->ident != NULL && strcmp(node_ptr->ident, "smkx") == 0)
					check_ti = true;
				else if (node_ptr->string != NULL && strcmp(node_ptr->string, smkx) == 0)
					check_ti = true;
				break;
			}
		}
	}

	for (node_ptr = map->mapping; node_ptr != NULL; node_ptr = node_ptr->next) {
		if ((other_node = lookup_node(map, node_ptr->key)) != node_ptr)
			error("%d: node %s:%s already defined on line %d\n", node_ptr->line_number, map->name, node_ptr->key, other_node->line_number);

		if (strcmp("%include", node_ptr->key) == 0)
			if (lookup_map(node_ptr->ident) == NULL)
				error("%d: %%include map %s not found\n", node_ptr->line_number, node_ptr->string);

		if (check_ti && (idx = get_ti_mapping(node_ptr->key)) >= 0) {
			char *tistr = tigetstr(keymapping[idx].tikey);
			if (!(tistr == (char *) -1 || tistr == NULL)) {
				if (node_ptr->string != NULL && strcmp(tistr, node_ptr->string) != 0)
					fprintf(stderr, "%d: warning: key %s:%s has a different definition than retrieved from the terminfo database (key %s)\n",
						node_ptr->line_number, map->name, node_ptr->key, keymapping[idx].tikey);
			}
		}
	}
}

static void check_maps(void) {
	t3_key_map_t *map_ptr, *other_map;

	for (map_ptr = maps; map_ptr != NULL; map_ptr = map_ptr->next) {
		if ((other_map = lookup_map(map_ptr->name)) != map_ptr)
			error("%d: map %s already defined on line %d\n", map_ptr->line_number, map_ptr->name, other_map->line_number);

		check_nodes(map_ptr);
	}
	if (best == NULL)
		error("No %%best key was found in the file\n");
}

static void fwrite_string(const char *string, FILE *output) {
	uint16_t out_short;
	out_short = htons(strlen(string));
	fwrite(&out_short, 1, 2, output);
	fwrite(string, 1, strlen(string), output);
}

static void write_nodes(t3_key_node_t *nodes, bool all_keys) {
	t3_key_node_t *node_ptr;
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
			fwrite_string(node_ptr->key, output);
			fwrite_string(string, output);
		}
	}
}

static void write_maps(void) {
	t3_key_map_t *map_ptr;
	const char magic[] = "CKEY";
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
		write_nodes(map_ptr->mapping, true);
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

	if (input == NULL)
		yyin = stdin;
	else if ((yyin = fopen(input, "r")) == NULL)
		fatal("Could not open input %s: %s\n", input, strerror(errno));
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
