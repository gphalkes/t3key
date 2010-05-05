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
CFLAGS+=-I. -I.objects

LDLIBS+=-lcurses

CFILES.t3learnkeys:= t3learnkeys.c
LCFILES.libt3key.la:= key.c
CFLAGS.key:= -fPIC -DDB_DIRECTORY=\"$(CURDIR)/database\"
CFILES.t3keyc:= t3keyc.c
GFILES.t3keyc:= grammar.g
LFILES.t3keyc:= lex.l
CFLAGS.lex:= -Wno-shadow -Wno-switch-default -Wno-unused
CFILES.test:= test.c
LDFLAGS.test:= -L.libs -lt3key


DEPS.grammar.g:= lex.l t3keyc.c

LDFLAGS.libt3key.so:= -shared

TARGETS:= t3learnkeys t3keyc libt3key.la test
EXTRATARGETS:= updatedb
#===========================================================================
.PHONY: all clean

all: $(TARGETS) $(EXTRATARGETS)

CFILES:= $(foreach PART, $(TARGETS), $(CFILES.$(PART)))
GFILES:= $(foreach PART, $(TARGETS), $(GFILES.$(PART)))
LFILES:= $(foreach PART, $(TARGETS), $(LFILES.$(PART)))
SOURCES:= $(CFILES) $(GFILES) $(LFILES)

# force use of our pattern rule for lex files
$(foreach FILE, $(LFILES), $(eval $(patsubst %.l, .objects/%.o, $(FILE)): $(patsubst %.l, .objects/%.c,$(FILE))))

OBJECTS:=$(CFILES:.c=.o) $(LFILES:.l=.o) $(GFILES:.g=.o) $(LCFILES:.c=.lo)
$(foreach PART, $(TARGETS), $(eval OBJECTS.$(PART):= \
	$$(patsubst %.c, .objects/%.o, $$(CFILES.$(PART))) \
	$$(patsubst %.g, .objects/%.o, $$(GFILES.$(PART))) \
	$$(patsubst %.l, .objects/%.o, $$(LFILES.$(PART))) \
	$$(patsubst %.c, .objects/%.lo, $$(LCFILES.$(PART)))))
DEPENDENCIES:= $(patsubst %, .deps/%, $(SOURCES))

$(foreach PART, $(filter-out lib%, $(TARGETS)), $(eval $(PART): $$(OBJECTS.$(PART)) ; \
	$$(CC) $$(CFLAGS) $$(CFLAGS.$(PART)) $$(LDFLAGS) $$(LDFLAGS.$(PART)) \
		-o $$@ $$^ $$(LDLIBS) $$(LDLIBS.$(PART))))

$(foreach PART, $(filter lib%, $(TARGETS)), $(eval $(PART): $$(OBJECTS.$(PART)) ; \
	libtool --mode=link $$(CC) $$(CFLAGS) $$(CFLAGS.$(PART)) $$(LDFLAGS) $$(LDFLAGS.$(PART)) \
		-o $$@ $$^ $$(LDLIBS) $$(LDLIBS.$(PART)) -rpath /usr/lib))
# Add dependency rules for grammar files. Header files generated from grammar
# files are needed by the lexical analyser and other files
$(foreach FILE, $(GFILES), $(if $(DEPS.$(FILE)), $(eval $(patsubst %.c, %.lo, $(patsubst %.l, %.c, $(patsubst %, .objects/%, $(DEPS.$(FILE))))): $(patsubst %.g, .objects/%.h, $(FILE)))))

.objects/%.o: %.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	$(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) -c $< -o $@
	@sed -i -r 's/\.o\>/\.lo/g' .deps/$<

.objects/%.o: .objects/%.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	$(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) -c $< -o $@
	@sed -i -r 's/\.o\>/\.lo/g' .deps/$<

.objects/%.lo: %.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	libtool --mode=compile $(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) $(LCFLAGS) -c $< -o $@
	@sed -i -r 's/\.o\>/\.lo/g' .deps/$<

.objects/%.lo: .objects/%.c
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	libtool --mode=compile $(CC) -MMD -MP -MF .deps/$< $(CFLAGS) $(CFLAGS.$*) $(LCFLAGS) -c $< -o $@
	@sed -i -r 's/\.o\>/\.lo/g' .deps/$<

.objects/%.c .objects/%.h: %.g
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	LLnextgen --base-name=.objects/$* $<

.objects/%.c: %.l
	@[ -d .deps/`dirname '$<'` ] || mkdir -p .deps/`dirname '$<'`
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	flex -o $@ $<

clean:
	rm -rf $(TARGETS) .deps .objects .libs >/dev/null 2>&1

-include $(DEPENDENCIES)
#===========================================================================
updatedb:
	@./updatedb

.objects/t3keyc.o: .objects/grammar.h

.PHONY: updatedb