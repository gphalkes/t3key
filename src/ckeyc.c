#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "optionMacros.h"

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

static const char *input;
extern FILE *yyin;

static PARSE_FUNCTION(parse_options)
	OPTIONS
		DOUBLE_DASH
			NO_MORE_OPTIONS;
		END_OPTION
		fatal("Unknown option " OPTFMT "\n", OPTPRARG);
	NO_OPTION
		if (input != NULL)
			fatal("Multiple input files specified\n");
		input = optArg;
	END_OPTIONS
END_FUNCTION


int main(int argc, char *argv[]) {
	parse_options(argc, argv);

	if (input == NULL)
		yyin = stdin;
	else if ((yyin = fopen(input, "r")) == NULL)
		fatal("Could not open input %s: %s\n", input, strerror(errno));

	return EXIT_SUCCESS;
}
