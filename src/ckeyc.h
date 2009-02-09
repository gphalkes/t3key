#ifndef CKEYC_H
#define CKEYC_H

#include <stdio.h>

void fatal(const char *fmt, ...);
extern FILE *yyin;
extern int line_number;
extern char *yytext;

#endif
