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
CFLAGS+=-I. -I.objects
LCFLAGS+=-DT3_KEY_BUILD_DSO

LDLIBS+=-lcurses

CFILES.t3learnkeys:= t3learnkeys.c
LCFILES.libt3key.la:= key.c
CFLAGS.key:= -fPIC -DDB_DIRECTORY=\"$(CURDIR)/database\"
CFILES.t3keyc:= t3keyc.c
GFILES.t3keyc:= grammar.g
LFILES.t3keyc:= lex.l
CFLAGS.lex:= -Wno-shadow -Wno-switch-default -Wno-unused
CFILES.test:= test.c
LDFLAGS.test:= -L.libs
LDLIBS.test:= -lt3key


DEPS.grammar.g:= lex.l t3keyc.c

LDFLAGS.libt3key.so:= -shared

TARGETS:= t3learnkeys t3keyc libt3key.la test
EXTRATARGETS:= updatedb

#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#
T3NAME:=key
include ../../t3shared/rules.mk

updatedb:
	@./updatedb

.objects/t3keyc.o: .objects/grammar.h
test: | libt3key.la

.PHONY: updatedb
