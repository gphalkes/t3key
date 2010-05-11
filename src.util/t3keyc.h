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
	char *ident;
	int line_number;
	t3_key_node_t *next;
};

typedef struct t3_key_map_t t3_key_map_t;
struct t3_key_map_t {
	char *name;
	t3_key_node_t *mapping;
	int line_number;
	t3_key_map_t *next;
};

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
