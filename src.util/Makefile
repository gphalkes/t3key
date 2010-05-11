VERSION?=debug
CC:=gcc
ifdef COVERAGE
	COVERAGEFLAGS:=-fprofile-arcs -ftest-coverage
endif
ifeq ($(VERSION),debug)
	CFLAGS:=-Wall -W -ggdb -DDEBUG -Wswitch-default \
		-Wcast-align -Wbad-function-cast \
		-Wcast-qual -Wwrite-strings -Wstrict-prototypes \
		$(COVERAGEFLAGS)
else
	CFLAGS:=-Wall -W $(ANSIFLAGS) -O2
endif
CFLAGS+=-I. -I../src -I.objects

LDLIBS+=-lcurses

CFILES.t3learnkeys:= t3learnkeys.c
CFILES.t3keyc:= t3keyc.c
GFILES.t3keyc:= grammar.g
LFILES.t3keyc:= lex.l
CFLAGS.lex:= -Wno-shadow -Wno-switch-default -Wno-unused
CFILES.test:= test.c
LDFLAGS.test:= -L../src/.libs
LDLIBS.test:= -lt3key

DEPS.grammar.g:= lex.l t3keyc.c

TARGETS:= t3learnkeys t3keyc test

#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#

.objects/test.o: | library

library:
	@$(MAKE) -C ../src libt3key.la

.PHONY: library
