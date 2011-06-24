# Copyright (C) 2010 G.P. Halkes
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
CFLAGS.key := -fPIC -DDB_DIRECTORY=\"$(CURDIR)/database\"

LTTARGETS := libt3key.la
EXTRATARGETS := updatedb

#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#
T3NAME := key
include ../../t3shared/rules.mk

CFLAGS += -DHAS_STRDUP

LDLIBS += -lcurses

updatedb: | utils
	@./updatedb

utils: | libt3key.la
	@$(MAKE) -C ../src.util $(_VERBOSE_PRINT)

.PHONY: updatedb utils
