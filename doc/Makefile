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

all: doxygen format.html format.txt
.PHONY: all

DOXYGENDEFS:=T3_KEY_CONST=const

T3NAME:=key
include ../../t3shared/doc-rules.mk

format.html: format.md
	sed -r 's/\\%/%/g;1s/\{.*//' $< | markdown > $@

format.txt: format.md
	sed -r 's/_num_/<num>/g;s/\\([_%])/\1/g;s%</?tt>%%g;s/&lt;/</g;s/&gt;/>/g;1s/\{.*//' $< > $@

clean::
	rm format.txt format.html
