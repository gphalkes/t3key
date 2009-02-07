# Copyright (C) 2007-2009 G.P. Halkes

#===========================================================================
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
CFLAGS+=-I.

LDLIBS+=-lcurses

CFILES.learnkeys:= learnkeys.c
CFILES.libckey:= ckey.c
CFLAGS.ckey:= -fPIC
CFILES.ckeyc:= ckeyc.c
GFILES.ckeyc:= grammar.g
LFILES.ckeyc:= lex.l
CFLAGS.lex:= -Wno-shadow -Wno-switch-default -Wno-unused

CFILES:= $(CFILES.learnkeys) $(CFILES.libckey) $(CFILES.ckeyc)
SOURCES:= $(CFILES) $(GFILES) $(LFILES)
#===========================================================================
.PHONY: all clean

all: learnkeys libckey.so ckeyc

OBJECTS:=$(CFILES:.c=.o) $(LFILES:.l=.o) $(GFILES:.g=.o)
OBJECTS.learnkeys:= $(patsubst %.c, .objects/%.o, $(CFILES.learnkeys))
OBJECTS.libckey:= $(patsubst %.c, .objects/%.o, $(CFILES.libckey))
OBJECTS.ckeyc:= $(patsubst %.c, .objects/%.o, $(CFILES.ckeyc)) $(patsubst %.l, .objects/%.o, $(LFILES.ckeyc)) $(patsubst %.g, .objects/%.o, $(GFILES.ckeyc))
DEPENDENCIES:= $(patsubst %, .deps/%, $(SOURCES))

learnkeys: $(OBJECTS.learnkeys)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

libckey.so: $(OBJECTS.libckey)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $@ $^ $(LDLIBS)

ckeyc: $(OBJECTS.ckeyc)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.objects/%.o: %.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	$(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) -c $< -o $@

.objects/%.o: .objects/%.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	$(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) -c $< -o $@

.objects/lex.o: grammar.h
.objects/ckeyc.o: grammar.h

%.c %.h: %.g
	LLnextgen $<

%.c: %.l
	flex -o $@ $<

clean:
	rm -rf learnkeys .deps .objects >/dev/null 2>&1

-include $(DEPENDENCIES)

#===========================================================================
.PHONY: linecount xgettext

linecount:
	@echo "WARNING: this doesn't count the header files, only .c files!"
	@echo "Linecounts as reported by wc -l"
	@wc -l $(CFILES)
	@if [ `which c_count 2>/dev/null` ] ; then echo "Linecounts as reported by the SLOCCount utilities" ; c_count $(CFILES) ; fi

xgettext: all
	xgettext -k_ -kN_ --add-comments=TRANSLATORS -o ../po/messages.pot `cat $$(find .deps -type f) | sed -r 's/^.+://;s/\\\\//g'`
