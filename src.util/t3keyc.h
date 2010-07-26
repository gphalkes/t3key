/* Copyright (C) 2010 G.P. Halkes
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
#ifndef T3KEYC_H
#define T3KEYC_H

#include <stdio.h>

void fatal(const char *fmt, ...);
char *safe_strdup(const char *str);
extern FILE *yyin;
extern int line_number;
extern char *yytext;

typedef struct t3_key_node_t t3_key_node_t;
struct t3_key_node_t {
	const char *key;
	char *string;
	size_t string_len;
	char *ident;
	int line_number;
	t3_key_node_t *next;
};

typedef struct t3_key_map_t t3_key_map_t;
struct t3_key_map_t {
	char *name;
	t3_key_node_t *mapping;
	int line_number;
	int flags;
	t3_key_map_t *next;
};

#define FLAG_MARK_INCLUDED (1<<0)
#define FLAG_MARK_LOOKUP (1<<1)

typedef struct t3_key_string_list_t t3_key_string_list_t;
struct t3_key_string_list_t {
	char *string;
	t3_key_string_list_t *next;
};

extern t3_key_map_t *maps;
extern char *best;
extern t3_key_string_list_t *akas;

t3_key_map_t *new_map(const char *name);
t3_key_node_t *new_node(const char *key, const char *string, const char *ident);
size_t parse_escapes(char *string);
#endif
