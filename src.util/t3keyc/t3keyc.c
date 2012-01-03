/* Copyright (C) 2012 G.P. Halkes
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
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <curses.h>
#include <term.h>

#include <t3config/config.h>

#include "optionMacros.h"

static const char map_schema[] = {
#include "map.bytes"
};

static bool option_link;
static bool option_trace_circular;
static const char *input;

/* Print usage message/help. */
static void print_usage(void) {
	printf("Usage: t3keyc [<OPTIONS>] <INPUT>\n"
		"  -h, --help                       Print this help message\n"
		"  -l, --link                       Create symbolic links for aliases\n"
		"  -t, --trace-circular-use         Trace circular 'use' inclusion\n");
	exit(EXIT_SUCCESS);
}

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

static void *safe_malloc(size_t size) {
	void *ptr;

	if ((ptr = malloc(size)) == NULL)
		fatal("Out of memory\n");
	return ptr;
}

/* Parse command line options */
static PARSE_FUNCTION(parse_options)
	OPTIONS
		OPTION('h', "help", NO_ARG)
			print_usage();
		END_OPTION
		OPTION('l', "link", NO_ARG)
			option_link = true;
		END_OPTION
		OPTION('t', "trace-circular-use", NO_ARG)
			option_trace_circular = true;
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

	if (option_link && option_trace_circular)
		fatal("-l/--link only valid without other options\n");

	if (input == NULL)
		fatal("No input\n");
END_FUNCTION


typedef struct map_list_t {
	t3_config_t *map;
	struct map_list_t *next;
} map_list_t;

static map_list_t *head;

static void check_circular_use_rec(t3_config_t *map_config, t3_config_t *map) {
	t3_config_t *use_name, *use_map;
	map_list_t *tmp, *ptr;

	tmp = safe_malloc(sizeof(map_list_t));
	tmp->map = map;
	tmp->next = head;
	head = tmp;

	for (use_name = t3_config_get(t3_config_get(map, "use"), NULL); use_name != NULL; use_name = t3_config_get_next(use_name)) {
		use_map = t3_config_get(t3_config_get(map_config, "maps"), t3_config_get_string(use_name));
		for (ptr = head; ptr != NULL; ptr = ptr->next) {
			if (ptr->map == use_map) {
				//FIXME: get file and line number info
				fprintf(stderr, "%s:%d: circular inclusion of map '%s'\n", input, 0, t3_config_get_name(use_map));
				if (option_trace_circular) {
					for (ptr = head; ptr != NULL; ptr = ptr->next) {
						fprintf(stderr, "  from map '%s'\n", t3_config_get_name(ptr->map));
						if (ptr->map == use_map)
							break;
					}
				}
				goto next_use;
			}
		}
		check_circular_use_rec(map_config, use_map);
next_use:;
	}

	tmp = head;
	head = tmp->next;
	free(tmp);
}

static void check_circular_use(t3_config_t *map_config) {
	t3_config_t *map;

	for (map = t3_config_get(t3_config_get(map_config, "maps"), NULL); map != NULL; map = t3_config_get_next(map)) {
		if (t3_config_get_name(map)[0] != '_')
			check_circular_use_rec(map_config, map);
	}
}

static void check_terminfo(t3_config_t *map_config) {
}

static void check_duplicates(t3_config_t *map_config) {
}

int main(int argc, char *argv[]) {
	t3_config_t *map_config;
	t3_config_opts_t opts;
	t3_config_error_t error;
	t3_config_schema_t *schema;
	FILE *file;

	parse_options(argc, argv);

	if ((file = fopen(input, "r")) == NULL)
		fatal("Could not open file '%s': %s\n", input, strerror(errno));

	opts.flags = T3_CONFIG_VERBOSE_ERROR;
	if ((map_config = t3_config_read_file(file, &error, &opts)) == NULL)
		fatal("%s:%d: %s%s%s\n", input, error.line_number, t3_config_strerror(error.error),
			error.extra == NULL ? "" : ": ", error.extra == NULL ? "" : error.extra);
	fclose(file);

	if ((schema = t3_config_read_schema_buffer(map_schema, sizeof(map_schema), &error, NULL)) == NULL)
		fatal("Internal schema contains an error: %s\n", t3_config_strerror(error.error));

	if (!t3_config_validate(map_config, schema, &error, T3_CONFIG_VERBOSE_ERROR))
		fatal("%s:%d: %s%s%s\n", input, error.line_number, t3_config_strerror(error.error),
			error.extra == NULL ? "" : ": ", error.extra == NULL ? "" : error.extra);

	if (option_link) {
		printf("NOT IMPLEMENTED YET\n");
		exit(1);
	}
	check_circular_use(map_config);
	check_terminfo(map_config);
	check_duplicates(map_config);

	/* FIXME: checks to implement:
		- map 'use's are not circular (although that should only trigger a warning)
		- each sequence only occurs once for each top-level map
		- definitions follow terminfo, unless the key is specifically named in noticheck
		- shiftfn values are sensible
	*/
	return 0;
}
