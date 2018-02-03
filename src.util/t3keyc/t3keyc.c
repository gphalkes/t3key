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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <ctype.h>
#include <curses.h>
#include <term.h>

#include <t3config/config.h>

#include "optionMacros.h"

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
#define is_asciidigit(x) ((x) >= '0' && (x) <= '9')
#define is_asciixdigit(x) \
  (is_asciidigit(x) || ((x) >= 'a' && (x) <= 'f') || ((x) >= 'A' && (x) <= 'F'))
#define to_asciilower(x) (((x) >= 'A' && (x) <= 'F') ? (x) - 'A' + 'a' : (x))

static const char map_schema[] = {
#include "map.bytes"
};

static bool option_link;
static bool option_trace_circular;
static bool option_check_terminfo = true;
static bool option_verbose;
static const char *input;

#include "mappings.c"

static const char *valid_names[] = {
    "insert",   "delete",    "home",     "end",      "page_up", "page_down",  "up",
    "left",     "down",      "right",    "kp_home",  "kp_up",   "kp_page_up", "kp_page_down",
    "kp_left",  "kp_center", "kp_right", "kp_end",   "kp_down", "kp_insert",  "kp_delete",
    "kp_enter", "kp_div",    "kp_mul",   "kp_minus", "kp_plus", "tab",        "backspace"};

/* Print usage message/help. */
static void print_usage(void) {
  printf(
      "Usage: t3keyc [<OPTIONS>] <INPUT>\n"
      "  -h, --help                       Print this help message\n"
      "  -l, --link                       Create symbolic links for aliases\n"
      "  -t, --trace-circular-use         Trace circular '_use' inclusion\n"
      "  -v, --verbose                    Verbose output\n");
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

  if ((ptr = malloc(size)) == NULL) {
    fatal("Out of memory\n");
  }
  return ptr;
}

static char *safe_strdup(const char *str) { return strcpy(safe_malloc(strlen(str) + 1), str); }

/* Parse command line options */
/* clang-format off */
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
    OPTION('v', "verbose", NO_ARG)
      option_verbose = true;
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
/* clang-format on */

/** Convert a string from the input format to an internally usable string.
        @param string A @a Token with the string to be converted.
        @return The length of the resulting string.

        The use of this function processes escape characters. The converted
        characters are written in the original string.
*/
static size_t parse_escapes(char *string) {
  size_t max_read_position = strlen(string);
  size_t read_position = 0, write_position = 0;
  size_t i;

  while (read_position < max_read_position) {
    if (string[read_position] == '\\') {
      read_position++;

      if (read_position == max_read_position) return 0;

      switch (string[read_position++]) {
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
          for (i = 0; i < 2 && (read_position + i) < max_read_position &&
                      is_asciixdigit(string[read_position + i]);
               i++) {
            value <<= 4;
            if (is_asciidigit(string[read_position + i]))
              value += (int)(string[read_position + i] - '0');
            else
              value += (int)(to_asciilower(string[read_position + i]) - 'a') + 10;
            if (value > UCHAR_MAX) return 0;
          }
          read_position += i;

          if (i == 0) return 0;

          string[write_position++] = (char)value;
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
          for (i = 0; i < max_idx && read_position + i < max_read_position &&
                      string[read_position + i] >= '0' && string[read_position + i] <= '7';
               i++)
            value = value * 8 + (int)(string[read_position + i] - '0');

          read_position += i;

          string[write_position++] = (char)value;
          break;
        }
        default:
          string[write_position++] = string[read_position - 1];
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

/* Convert a sequence to a printable representation. */
static char *get_print_seq(const char *seq) {
  static char buffer[1024];
  char *dest = buffer;

  while (*seq && dest - buffer < 1018) {
    if (*seq == '"') {
      *dest++ = '\\';
      *dest++ = '"';
      seq++;
    } else if (*seq == '\\') {
      *dest++ = *seq;
      *dest++ = *seq++;
    } else if (isprint(*seq)) {
      *dest++ = *seq++;
    } else if (*seq == 27) {
      *dest++ = '\\';
      *dest++ = 'e';
      seq++;
    } else {
      sprintf(dest, "\\%03o", *seq++);
      dest += 4;
    }
  }
  *dest = 0;
  return buffer;
}

typedef struct map_list_t {
  t3_config_t *map;
  struct map_list_t *next;
} map_list_t;

typedef struct sequence_list_t {
  t3_config_t *config;
  char *str;
  size_t str_len;
  struct sequence_list_t *next;
} sequence_list_t;

static map_list_t *map_head;
static sequence_list_t *seq_head;

static t3_bool compare_name(const t3_config_t *check, const void *check_value) {
  const char *str = t3_config_get_string(check);
  if (str == NULL) return t3_false;
  return strcmp(check_value, str) == 0;
}

static void add_sequence(t3_config_t *ptr, t3_config_t *noticheck) {
  sequence_list_t *seq_ptr;
  const char *name, *minus;
  size_t name_len;
  size_t i;
  char *str = safe_strdup(t3_config_get_string(ptr));
  size_t str_len = parse_escapes(str);
  bool invalid_name = false;

  name = t3_config_get_name(ptr);
  minus = strchr(name, '-');
  name_len = minus == NULL ? strlen(name) : (size_t)(minus - name);
  for (i = 0; i < ARRAY_LENGTH(valid_names); i++) {
    if (strncmp(name, valid_names[i], name_len) == 0 && name_len == strlen(valid_names[i])) {
      break;
    }
  }

  if (i == ARRAY_LENGTH(valid_names)) {
    if (!(name[0] == 'f' && is_asciidigit(name[1]) &&
          (name[2] == 0 || name[2] == '-' ||
           (is_asciidigit(name[2]) && (name[3] == 0 || name[3] == '-') && name[1] != '0')))) {
      // FIXME: get file name information
      fprintf(stderr, "%s:%d: '%s' is not a valid key name\n", input,
              t3_config_get_line_number(ptr), t3_config_get_name(ptr));
      invalid_name = true;
    }
  }

  if (minus != NULL && strcmp(minus + 1, "s") != 0 && strcmp(minus + 1, "m") != 0 &&
      strcmp(minus + 1, "c") != 0 && strcmp(minus + 1, "cm") != 0 && strcmp(minus + 1, "cs") != 0 &&
      strcmp(minus + 1, "ms") != 0 && strcmp(minus + 1, "cms") != 0) {
    fprintf(stderr, "%s:%d: '%s' includes incorrect modifier sequence\n", input,
            t3_config_get_line_number(ptr), t3_config_get_name(ptr));
    invalid_name = true;
  }

  for (seq_ptr = seq_head; seq_ptr != NULL; seq_ptr = seq_ptr->next) {
    // FIXME: get file name information
    if (seq_ptr->str_len == str_len && memcmp(seq_ptr->str, str, str_len) == 0) {
      fprintf(stderr, "%s:%d: '%s' has the same sequence as '%s' defined at %s:%d\n", input,
              t3_config_get_line_number(ptr), t3_config_get_name(ptr),
              t3_config_get_name(seq_ptr->config), input,
              t3_config_get_line_number(seq_ptr->config));
      break;
    }
  }

  if (seq_ptr == NULL) {
    seq_ptr = safe_malloc(sizeof(sequence_list_t));
    seq_ptr->config = ptr;
    seq_ptr->str = str;
    seq_ptr->str_len = str_len;
    seq_ptr->next = seq_head;
    seq_head = seq_ptr;
  }

  if (option_check_terminfo && !invalid_name &&
      t3_config_find(noticheck, compare_name, name, NULL) == NULL) {
    const char *tistr;
    for (i = 0; i < ARRAY_LENGTH(keymapping); i++) {
      if (strcmp(name, keymapping[i].key) == 0) {
        tistr = tigetstr(keymapping[i].tikey);
        break;
        return;
      }
    }
    if (i == ARRAY_LENGTH(keymapping)) {
      char buffer[100];
      if (!(name[0] == 'f' && is_asciidigit(name[1]))) {
        return;
      }
      if (minus != NULL) {
        return;
      }
      buffer[0] = 'k';
      strcpy(buffer + 1, name);
      tistr = tigetstr(buffer);
    }

    if (tistr != (char *)0 && tistr != (char *)-1) {
      if (strlen(tistr) != str_len || memcmp(tistr, str, str_len) != 0) {
        // FIXME: get file name information
        fprintf(stderr, "%s:%d: '%s' has different definition (%s) than terminfo (%s)\n", input,
                t3_config_get_line_number(ptr), t3_config_get_name(ptr), t3_config_get_string(ptr),
                get_print_seq(tistr));
      }
    }
  }
}

static void add_sequences(t3_config_t *map) {
  t3_config_t *noticheck, *ptr;

  noticheck = t3_config_get(map, "_noticheck");

  for (ptr = t3_config_get(map, NULL); ptr != NULL; ptr = t3_config_get_next(ptr)) {
    const char *name = t3_config_get_name(ptr);

    if (strcmp(name, "_enter") == 0 || strcmp(name, "_leave") == 0) {
      if (t3_config_get_string(ptr)[0] == '\\') {
        char *str = safe_strdup(t3_config_get_string(ptr));
        parse_escapes(str);
        free(str);
      } else {
        char *tistr = tigetstr(t3_config_get_string(ptr));
        if (tistr == (char *)0 || tistr == (char *)-1) {
          // FIXME: get file name information
          fprintf(stderr, "%s:%d: '%s' specifies non-%s terminfo entry '%s'\n", input,
                  t3_config_get_line_number(ptr), t3_config_get_name(ptr),
                  tistr == (char *)0 ? "existant" : "string", t3_config_get_string(ptr));
        }
      }
    } else if (name[0] != '_') {
      add_sequence(ptr, noticheck);
    }
  }
}

static void check_map_rec(t3_config_t *map_config, t3_config_t *map, bool outer);

static void check_use(t3_config_t *map_config, t3_config_t *use_map) {
  map_list_t *ptr;
  for (ptr = map_head; ptr != NULL; ptr = ptr->next) {
    if (ptr->map == use_map) {
      // FIXME: get file name info
      fprintf(stderr, "%s:%d: circular inclusion of map '%s'\n", input,
              t3_config_get_line_number(use_map), t3_config_get_name(use_map));
      if (option_trace_circular) {
        for (ptr = map_head; ptr != NULL; ptr = ptr->next) {
          fprintf(stderr, "  from map '%s'\n", t3_config_get_name(ptr->map));
          if (ptr->map == use_map) break;
        }
      }
      return;
    }
  }
  check_map_rec(map_config, use_map, false);
}

static void check_map_rec(t3_config_t *map_config, t3_config_t *map, bool outer) {
  t3_config_t *use_name, *use_map, *enter_leave;
  map_list_t *tmp;
  bool saved_option_check_terminfo = option_check_terminfo;

  tmp = safe_malloc(sizeof(map_list_t));
  tmp->map = map;
  tmp->next = map_head;
  map_head = tmp;

  if (!outer) {
    if ((enter_leave = t3_config_get(map, "_enter")) != NULL)
      fprintf(stderr, "%s:%d: 'enter' should only be used in top-level maps\n", input,
              t3_config_get_line_number(enter_leave));
    if ((enter_leave = t3_config_get(map, "_leave")) != NULL)
      fprintf(stderr, "%s:%d: 'leave' should only be used in top-level maps\n", input,
              t3_config_get_line_number(enter_leave));
  } else {
    if (((enter_leave = t3_config_get(map, "_enter")) == NULL ||
         strcmp(t3_config_get_string(enter_leave), "smkx") != 0) &&
        !t3_config_get_bool(t3_config_get(map, "_ticheck"))) {
      option_check_terminfo = false;
    }
  }

  add_sequences(map);

  for (use_name = t3_config_get(t3_config_get(map, "_use"), NULL); use_name != NULL;
       use_name = t3_config_get_next(use_name)) {
    use_map = t3_config_get(t3_config_get(map_config, "maps"), t3_config_get_string(use_name));
    check_use(map_config, use_map);
  }

  tmp = map_head;
  map_head = tmp->next;
  free(tmp);

  if (outer) {
    option_check_terminfo = saved_option_check_terminfo;
  }
}

static void check_maps(t3_config_t *map_config) {
  t3_config_t *map;

  for (map = t3_config_get(t3_config_get(map_config, "maps"), NULL); map != NULL;
       map = t3_config_get_next(map)) {
    if (t3_config_get_name(map)[0] == '_') continue;

    check_map_rec(map_config, map, true);

    while (seq_head != NULL) {
      sequence_list_t *tmp = seq_head;
      seq_head = tmp->next;
      free(tmp->str);
      free(tmp);
    }
  }
}

static void create_symlinks(t3_config_t *map_config, const char *term_name) {
  const t3_config_t *aka;
  char *dirname = safe_strdup(input);
  char *linkname;

  if (strrchr(dirname, '/') != NULL) {
    strrchr(dirname, '/')[1] = 0;
  }

  for (aka = t3_config_get(t3_config_get(map_config, "aka"), NULL); aka != NULL;
       aka = t3_config_get_next(aka)) {
    linkname = safe_malloc(strlen(dirname) + strlen(t3_config_get_string(aka)) + 1);
    strcpy(linkname, dirname);
    strcat(linkname, t3_config_get_string(aka));
    if (symlink(term_name, linkname) == -1) {
      if (errno != EEXIST || option_verbose)
        fprintf(stderr, "Could not create symbolic link %s -> %s: %s\n", linkname, term_name,
                strerror(errno));
    }
    free(linkname);
  }
  free(dirname);
}

int main(int argc, char *argv[]) {
  t3_config_t *map_config;
  t3_config_opts_t opts;
  t3_config_error_t error;
  t3_config_schema_t *schema;
  const char *term_name;
  int err;
  FILE *file;

  parse_options(argc, argv);

  if ((file = fopen(input, "r")) == NULL) {
    fatal("Could not open file '%s': %s\n", input, strerror(errno));
  }

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
  t3_config_delete_schema(schema);

  if ((term_name = strrchr(input, '/')) == NULL) {
    term_name = input;
  } else {
    term_name++;
  }

  if (option_link) {
    create_symlinks(map_config, term_name);
    exit(EXIT_SUCCESS);
  }

  if (setupterm(term_name, 1, &err) == ERR) {
    fprintf(stderr, "Could not find terminfo for %s\n", term_name);
    option_check_terminfo = false;
  }
  check_maps(map_config);

  t3_config_delete(map_config);
  return EXIT_SUCCESS;
}
