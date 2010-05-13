CFILES.t3learnkeys := t3learnkeys.c
CFILES.t3keyc := t3keyc.c
GFILES.t3keyc := grammar.g
LFILES.t3keyc := lex.l
CFLAGS.lex := -Wno-shadow -Wno-switch-default -Wno-unused
CFILES.test := test.c
LDFLAGS.test := -L../src/.libs
LDLIBS.test := -lt3key

DEPS.grammar.g := lex.l t3keyc.c

TARGETS := t3learnkeys t3keyc test

#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#
CFLAGS += -I. -I../src -I.objects
LDLIBS.t3learnkeys += -lcurses
LDLIBS.t3keyc += -lcurses

.objects/test.o: | library

library:
	@$(MAKE) -C ../src $(_VERBOSE_PRINT) libt3key.la

.PHONY: library
