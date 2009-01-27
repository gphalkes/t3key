# Copyright (C) 2007-2008 G.P. Halkes

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

LDFLAGS+=-lcurses

CFILES:= learnkeys.c
#===========================================================================
.PHONY: all clean

all: learnkeys

OBJECTS:= $(patsubst %.c, .objects/%.o, $(CFILES))
DEPENDENCIES:= $(patsubst %, .deps/%, $(CFILES))

learnkeys: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.objects/%.o: %.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	$(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) -c $< -o $@

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
