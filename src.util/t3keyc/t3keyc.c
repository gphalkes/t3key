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
static const char *input;

/* Print usage message/help. */
static void print_usage(void) {
	printf("Usage: t3keyc [<OPTIONS>] <INPUT>\n"
		"  -l, --link                       Create symbolic links for aliases\n"
		"  -h, --help                       Print this help message\n");
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

/* Parse command line options */
static PARSE_FUNCTION(parse_options)
	OPTIONS
		OPTION('l', "link", NO_ARG)
			option_link = true;
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

	if (input == NULL)
		fatal("No input\n");
END_FUNCTION




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

	/* FIXME: checks to implement:
		- map 'use's are not circular (although that should only trigger a warning)
		- each sequence only occurs once for each top-level map
		- definitions follow terminfo, unless the key is specifically named in noticheck
		- shiftfn values are sensible
	*/
	return 0;
}
