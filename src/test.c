#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <term.h>

#include "ckey.h"

int main(int argc, char *argv[]) {
	setupterm("xterm", 1, NULL);
	printf("%p\n", ckey_load_map("xterm", NULL, NULL));
	return 0;
}
