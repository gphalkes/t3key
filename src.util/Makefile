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

SOURCES.t3learnkeys := t3learnkeys.c
LDLIBS.t3learnkeys := -lcurses -lX11
SOURCES.t3keyc := t3keyc.c grammar.g lex.l
LDLIBS.t3keyc := -lcurses
CFLAGS.lex := -Wno-shadow -Wno-switch-default -Wno-unused
SOURCES.test := test.c
LDFLAGS.test := -L../src/.libs
LDLIBS.test := -lt3key

DEPS.grammar.g := lex.l t3keyc.c

TARGETS := t3learnkeys t3keyc test

#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#
CFLAGS += -I. -I../src -I.objects
FLFLAGS += --never-interactive --nounistd

.objects/test.o: | library

library:
	@$(MAKE) -C ../src $(_VERBOSE_PRINT) libt3key.la

.PHONY: library
