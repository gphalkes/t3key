#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <term.h>
#include <ctype.h>
#include <string.h>

#include "ckey.h"

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

	setupterm(term, 1, NULL);
	node = t3_key_load_map(term, NULL, &error);
	printf("%s: %s\n", term, t3_key_strerror(error));

	while (node) {
		printf("%s = ", node->key);
		write_escaped_string(stdout, node->string, strlen(node->string));
		printf("\n");
		node = node->next;
	}

	return 0;
}
