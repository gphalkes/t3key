# Copyright (C) 2011,2018 G.P. Halkes
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3, as
# published by the Free Software Foundation.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

SOURCES.libt3key.la := key.c key_shared.c

LTTARGETS := libt3key.la
EXTRATARGETS := updatedblinks
#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#
T3NAME := key
include ../../t3shared/rules.mk
include ../../t3shared/rules-base.mk

CFLAGS += -fPIC -DDB_DIRECTORY=\"$(CURDIR)/database\"
CFLAGS += -DHAS_STRDUP
CFLAGS.key := -I.objects

LDLIBS += -lcurses
LDLIBS += -lt3config
LDFLAGS += $(T3LDFLAGS.t3config)

key.c: .objects/map.bytes

.objects/%.bytes: %.schema
	$(GENOBJDIR)
	$(_VERBOSE_GEN) ../../t3config/src/data2bytes -s -- $< > $@

utils: | libt3key.la
	@$(MAKE) -C ../src.util $(_VERBOSE_PRINT)

updatedblinks: utils
	@for i in database/* ; do ../src.util/t3keyc/t3keyc -l $$i ; done

clang-format:
	clang-format -i *.c *.h

.PHONY: updatedblinks utils links clang-format
