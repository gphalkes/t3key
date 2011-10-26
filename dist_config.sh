PACKAGE=libt3key
SRCDIRS="src src.util"
EXCLUDESRC="/(Makefile|TODO.*|SciTE.*|run\.sh|test\.c)$"
GENSOURCES="`echo src.util/t3keyc/.objects/*.{c,h}` src/key_api.h src/key_errors.h src/key_shared.c"
AUXSOURCES="src.util/t3keyc/.objects/mappings.c"
VERSIONINFO="0:0:0"
