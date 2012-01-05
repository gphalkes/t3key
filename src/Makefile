# Copyright (C) 2011 G.P. Halkes
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
include ../../shared/rules.mk

CFLAGS += -fPIC -DDB_DIRECTORY=\"$(CURDIR)/database\"
CFLAGS += -DHAS_STRDUP
CFLAGS.key := -I.objects

LDLIBS += -lcurses
LDLIBS += -Lt3config/.libs -lt3config

key.c: .objects/map.bytes

.objects/%.bytes: %.schema
	$(GENOBJDIR)
	$(_VERBOSE_GEN) ./t3config/data2bytes -s -- $< > $@

utils: | libt3key.la
	@$(MAKE) -C ../src.util $(_VERBOSE_PRINT)

links:
	@[ -L t3key ] || ln -s . t3key
	@[ -L t3config ] || ln -s ../../config/src t3config

updatedblinks: utils
	@for i in database/* ; do LD_LIBRARY_PATH=t3config/.libs ../src.util/t3keyc/t3keyc -l $$i ; done

.PHONY: updatedblinks utils links
