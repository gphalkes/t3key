LCFILES.libt3key.la := key.c key_shared.c
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

LDLIBS += -lcurses

updatedb: | utils
	@./updatedb

utils: | libt3key.la
	@$(MAKE) -C ../src.util $(_VERBOSE_PRINT)

.PHONY: updatedb utils
