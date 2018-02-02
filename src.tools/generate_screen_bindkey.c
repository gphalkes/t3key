/* Copyright (C) 2013 G.P. Halkes
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
#include <ctype.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>

#include "t3key/key.h"

typedef struct map_t {
  const char *name;
  const t3_key_node_t *map;
  struct map_t *next;
} map_t;

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

void write_escaped_string(FILE *out, const char *string, size_t length) {
  size_t i;

  fputc('"', out);
  for (i = 0; i < length; i++, string++) {
    if (!isprint(*string)) {
      fprintf(out, "\\%03o", (unsigned char)*string);
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
  const t3_key_node_t *screen_map, *screen_node, *node;
  map_t *all_maps = NULL, *other_map;
  int i, error, fail = 0;

  if (argc == 1 || (argc == 2 && strcmp(argv[1], "-h") == 0)) {
    printf("Usage: generate_screen_bindkey [TERMINAL NAME]...\n");
    exit(EXIT_SUCCESS);
  }

  screen_map = t3_key_load_map("screen", NULL, &error);
  if (screen_map == NULL) fatal("Could not load map for screen\n");

  for (i = 1; i < argc; i++) {
    const t3_key_node_t *map = t3_key_load_map(argv[i], NULL, &error);
    map_t *new_map;

    if (map == NULL) fatal("Could not load map for terminal %s\n", argv[i]);

    for (node = map; node != NULL; node = node->next) {
      const t3_key_node_t *other_node;

      if (node->string == NULL) continue;

      for (screen_node = screen_map; screen_node != NULL; screen_node = screen_node->next) {
        if (strcmp(node->key, screen_node->key) == 0) break;
      }
      if (screen_node == NULL) continue;

      for (other_map = all_maps; other_map != NULL; other_map = other_map->next) {
        for (other_node = other_map->map; other_node != NULL; other_node = other_node->next) {
          if (other_node->string == NULL || other_node->string_length != node->string_length ||
              strncmp(other_node->string, node->string, node->string_length) != 0 ||
              strcmp(other_node->key, node->key) == 0)
            continue;
          fprintf(stderr, "Colliding key found for %s:%s with %s:%s: ", argv[i], node->key,
                  other_map->name, other_node->key);
          write_escaped_string(stderr, node->string, node->string_length);
          fprintf(stderr, "\n");
          fail = 1;
        }
      }
    }
    if ((new_map = malloc(sizeof(map_t))) == NULL) fatal("Out of memory\n");
    new_map->name = argv[i];
    new_map->map = map;
    new_map->next = all_maps;
    all_maps = new_map;
  }

  if (fail) exit(EXIT_FAILURE);

  /* FIXME: skip the default key bindings for screen, like up/down/left/right. */
  for (other_map = all_maps; other_map != NULL; other_map = other_map->next) {
    for (node = other_map->map; node != NULL; node = node->next) {
      if (node->string == NULL) continue;

      for (screen_node = screen_map; screen_node != NULL; screen_node = screen_node->next) {
        if (strcmp(node->key, screen_node->key) == 0) break;
      }
      if (screen_node == NULL) continue;

      if (screen_node->string_length == node->string_length &&
          strncmp(screen_node->string, node->string, node->string_length) == 0)
        continue;

      printf("bindkey ");
      write_escaped_string(stdout, node->string, node->string_length);
      printf(" stuff ");
      write_escaped_string(stdout, screen_node->string, screen_node->string_length);
      printf("\n");
    }
  }
  return EXIT_SUCCESS;
}
