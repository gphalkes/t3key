#!/bin/bash
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

# This script extracts the common keys from a key map and creates a new map
# which uses a shared map for the common keys

IFS=$'\n'

SHARED=`mktemp sharedXXXXXX`
REMOVED=`mktemp removedXXXXXX`
TMP=`mktemp tmpXXXXXX`

sed 's/\\/\\\\/g' $1 | while read LINE ; do
	if [[ "$LINE" =~ "no sequence" ]] || [[ "$LINE" =~ "}" ]] ; then
		continue;
	fi

	if fgrep -q -x "$LINE" "$SHARED" ; then
		continue;
	fi

	TIMES=`fgrep -c -x "$LINE" $1`
	if [ "$TIMES" -ge 2 ] ; then
		echo "$LINE" >> "$SHARED"
	fi
done

cp $1 "$REMOVED"
sed 's/\\/\\\\/g' "$SHARED" | while read LINE ; do
	fgrep -x -v "$LINE" "$REMOVED" > "$TMP"
	mv "$TMP" "$REMOVED"
done

echo "_shared {"
cat "$SHARED"
echo "}"
cat "$REMOVED"

rm "$SHARED"
rm "$REMOVED"
