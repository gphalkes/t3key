#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <term.h>
#include <ctype.h>
#include <string.h>

#include "ckey.h"

#define TERM_STRING "rxvt"

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
	const CKeyNode *node;
	setupterm(TERM_STRING, 1, NULL);
	printf("%p\n", node = ckey_load_map(TERM_STRING, NULL, NULL));

	while (node) {
		printf("%s = ", node->key);
		write_escaped_string(stdout, node->string, strlen(node->string));
		printf("\n");
		node = node->next;
	}

	return 0;
}
