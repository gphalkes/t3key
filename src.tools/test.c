/* Copyright (C) 2011 G.P. Halkes
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
#include <stdlib.h>
#include <curses.h>
#include <term.h>
#include <ctype.h>
#include <string.h>

#include "t3key/key.h"

void write_escaped_string(FILE *out, const char *string, size_t length) {
	size_t i;

	fputc('"', out);
	for (i = 0; i < length; i++, string++) {
		if (!isprint(*string)) {
			fprintf(out, "\\%03o", (unsigned char) *string);
		} else if (*string == '\\' || *string == '"') {
			fputc('\\', out);
			fputc(*string, out);
		} else {
			fputc(*string, out);
		}
	}
	fputs("\"", out);
}

int main(int argc, char *argv[]) {
	const t3_key_node_t *node;
	const char *term = NULL;
	int error;

	if (argc > 2 || (argc == 2 && strcmp(argv[1], "-h") == 0)) {
		printf("Usage: test [<terminal name>]\n");
		exit(EXIT_SUCCESS);
	}

	if (argc == 2)
		term = argv[1];

	node = t3_key_load_map(term, NULL, &error);
	printf("%s: %s\n", term, t3_key_strerror(error));

	while (node) {
		printf("%s", node->key);
		if (node->string != NULL) {
			printf(" = ");
			write_escaped_string(stdout, node->string, node->string_length);
		}
		printf("\n");
		node = node->next;
	}

	return 0;
}
