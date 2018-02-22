# Copyright (C) 2012,2018 G.P. Halkes
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3, as
# published by the Free Software Foundation.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

SOURCES.t3keyc := t3keyc.c

TARGETS := t3keyc
#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../../makesys/rules.mk
#================================================#
include ../../../t3shared/rules-base.mk

CFLAGS += -I. -I../../src -I.objects -I.. -I../../src/.objects

LDFLAGS += $(T3LDFLAGS.t3config)
LDLIBS += -lcurses
LDLIBS += -lt3config

.objects/mappings.c: ../../src/key.c
	@[ -d .objects/`dirname '$<'` ] || mkdir -p .objects/`dirname '$<'`
	$(_VERBOSE_GEN) { echo "/* Generated from src/key.c */" ; sed -r '1,\%/\*START MAPPINGS\*/%d;\%/\*END MAPPINGS\*/%,$$d' $< ; } > $@

.objects/t3keyc.o: .objects/mappings.c
.objects/t3keyc.o: ../../src/.objects/map.bytes

../../src/.objects/map.bytes: ../../src/map.schema
	$(MAKE) -C ../../src .objects/map.bytes

clang-format:
	clang-format -i *.[ch]

.PHONY: clang-format
