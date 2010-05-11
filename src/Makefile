LCFILES.libt3key.la := key.c
CFLAGS.key := -fPIC -DDB_DIRECTORY=\"$(CURDIR)/database\"

TARGETS := libt3key.la
EXTRATARGETS := updatedb

#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#
T3NAME := key
include ../../t3shared/rules.mk

CFLAGS += -I.
LCFLAGS += -DT3_KEY_BUILD_DSO
LDLIBS += -lcurses

updatedb: | utils
	@./updatedb

utils: | libt3key.la
	@$(MAKE) -C ../src.util

.PHONY: updatedb utils
