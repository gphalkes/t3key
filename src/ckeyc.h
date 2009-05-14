#ifndef CKEYC_H
#define CKEYC_H

#include <stdio.h>

void fatal(const char *fmt, ...);
char *safe_strdup(const char *str);
extern FILE *yyin;
extern int line_number;
extern char *yytext;

typedef struct CKeyNode CKeyNode;
struct CKeyNode {
	const char *key;
	char *string;
	char *terminfo_name;
	int line_number;
	CKeyNode *next;
};

typedef struct CKeyMap CKeyMap;
struct CKeyMap {
	char *name;
	CKeyNode *mapping;
	int line_number;
	CKeyMap *next;
};

extern CKeyMap *maps;
extern char *best;

CKeyMap *new_map(const char *name);
CKeyNode *new_node(const char *key, const char *string, const char *terminfo_name);

#endif
