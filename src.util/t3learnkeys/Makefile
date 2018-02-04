# Copyright (C) 2010,2018 G.P. Halkes
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3, as
# published by the Free Software Foundation.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

SOURCES.t3learnkeys := t3learnkeys.c x11.c
LDLIBS.t3learnkeys := -lcurses

TARGETS := t3learnkeys

#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../../makesys/rules.mk
#================================================#
CFLAGS += -I..
CFLAGS += -DHAS_SELECT_H
#~ CFLAGS += -DNO_AUTOLEARN

#~ CFLAGS += -DUSE_XLIB
#~ CFLAGS += `pkg-config --cflags x11`
#~ LDLIBS += `pkg-config --libs x11`

CFLAGS += `pkg-config --cflags xcb`
LDLIBS += `pkg-config --libs xcb`

clang-format:
	clang-format -i *.c *.h

.PHONY: clang-format
