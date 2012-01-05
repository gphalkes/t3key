PACKAGE=libt3key
SRCDIRS="src src.util"
EXCLUDESRC="/(Makefile|TODO.*|SciTE.*|run\.sh|test\.c)$"
GENSOURCES="`echo src.util/t3keyc/.objects/*.c` src/.objects/map.bytes src/key_api.h src/key_errors.h src/key_shared.c"
VERSIONINFO="1:0:0"
