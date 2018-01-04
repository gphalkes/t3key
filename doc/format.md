Database Format  {#database-format}
===============

Preliminaries
-------------

The file format syntax is defined by libt3config. Strings are stored as text
delimted by double or single quotes, with quote duplication to include literal
quotes and with backslash escaping to include special characters. All values are
stored as key-value pairs. Keys start with either a letter (lowercase) or an
underscore. Second and later key characters can be a letter, a number or an
underscore. Keys starting with a percent sign indicate items in a list, and may
occur multiple times. Any characters after a hash (#) character that is not part
of a string up to the end of the line are considered comments and are ignored.

For a more thorough description of the syntax, please refer to the
[libt3config documentation](https://os.ghalkes.nl/doc/libt3config).

When describing keyboard keys, modifiers are encoded in the key-value key by
by adding a minus sign (-) followed by one or more of the letters <tt>c</tt>,
<tt>m</tt> and <tt>s</tt> (in that order, for control, meta and shift
respectively).

Maps
----

A map is defined by a key-value key followed by an open brace, a set of
key-value pairs and finaly a closing brace. The key-value pairs in the map are
specified as key = value. The permissable keys consist of the special keys
<tt>\%\_use</tt>, <tt>\_enter</tt> and <tt>\_leave</tt>, and the names for the
different keys on the keyboard (as specified in a later section), optionally
with modifiers.

Maps may only be defined in the maps section. For example:

	maps {
	    kx {
	        home = "\e[O"
	    }
	}

The <tt>\%\_use</tt> special key can be used to include the key-value pairs of
the named map into the current map. The value for <tt>\%\_use</tt> should be a
list of map quoted names. <tt>\_enter</tt> and <tt>\_leave keys</tt> need not be
present in the included map. Maps whose name begins with and underscore are
internal maps, and should only be used for inclusion in other maps.
<tt>\%\_use</tt> may occur multiple times to include different maps.

The <tt>\_enter</tt> key defines the string to send to the terminal to switch
the terminal to the mode for which this map defines the keys. The
<tt>\_leave</tt> key defines the string to reset the terminal to its original
state. For the <tt>\_enter</tt> and <tt>\_leave</tt> keys, the value can be
either a terminfo name or a string literal starting with a backslash.

Normal keys should have a string as their value. If a non-keypad and a keypad
key share both function and string, only the non-keypad key should be included.

Application Keypad Mode
-----------------------

Many terminals provide a so-called Application Keypad (kx) Mode. In this mode,
the keys that form the keypad (the numeric entry block, but with NumLock
disabled) may send different sequences from the regular cursor keys. Although it
is generally inadvisable to rely on the distinction between these keys,
switching on Application Keypad Mode for some terminals results in richer
information about keys being pressed. For example, the terminal may report
modifiers like shift or control in combination with arrow keys.

For this reason, it is advisable to record the keys for both the regular mode
(a.k.a. nokx) and Application Keypad (kx) mode.

Best map
--------

The "best map", as indicated by the <tt>best</tt> key, should indicate the map
that gives access to the most useful set of key mappings. What "most useful"
means is unfortunately not easy to define. Being able to distinghuish between as
many combinations of keys and modifiers is of course desirable, but not always
the only consideration.

The value associated with the <tt>best</tt> key is the quoted name of a map
defined in the file. The best key should be defined outside any map and is
mandatory. Most commonly, it will be set to "kx" (see Application Keypad Mode
above).

Also Known As
-------------

Many maps are shared by different terminals. For example, the same map is used
by both <tt>xterm</tt> and <tt>xterm-256color</tt>. By including an <tt>aka</tt>
list, these shared maps can be included under different names. The practical
implementation is that a (symbolic) link will be created from the original name
to each name specified in the aka list, when using <tt>t3keyc</tt> (see below).

The values in an aka list should be additional names under which this map
should be known.

Shift FN
--------

Some terminals map (a limited range of) shifted function keys to higher
numbered functions keys. For example, pressing shift-F1 in rxvt produces the
same escape sequence as pressing F11. Because not all terminals do this, or use
the same mapping, this information can be specified through the <tt>shiftfn</tt>
key.

The <tt>shiftfn</tt> key expects a list with three comma separated numbers. The
first number represents the base of the mapped range. This is usually 1. The
second number represents the end of the mapped range. Common values for this are
10 and 12. The third number represents the mapping of the first function key as
indicated by the first number. For example, the list ( 1, 10, 11 ) indicates
that pressing shift-F1 will be mapped to F11, shift-F2 will be mapped to F12,
up to shift-F10, which will be mapped to F20.

The reason to include this information is to indicate to client programs that
the terminal does not produce separate escape sequences for the indicated range
of shifted function keys, but it may use the mapped range as an alternative if
it so desires. Most current keyboards do not provide more than 12 function
keys, but pressing shift in combination with a function key is a common
operation. So a client program can choose to interpret the higher numbered
function keys as shifted low-numbered function keys instead, thereby providing
the expected behavior when the user presses a shifted function key.

XTerm mouse reporting
---------------------

Several terminal emulators support the XTerm mouse reporting protocol, or a
subset. There are three different things that a terminal emulator may support:

- Basic button up/down reporting, enabled by ESC [?1000h
- Button up/down reporting with motion reporting if a button is down, enabled
  by ESC [?1002h
- Full reporting (i.e. buttons and motion all the time), enabled by ESC [?1003h
- Extended coordinate mode for the previous modes, enabled by ESC [?1005h

If any of these is supported, you can add <tt>xterm\_mouse = true</tt> to the
description of the terminal emulator.

When enabling the mouse modes from a client program (provided the terminal
emulator supports it) the enable strings should be sent, in the above order:
ESC [?1000h ESC [?1002h ESC [?1005h

This will ensure that the best mode is automatically selected.

Valid key names
---------------

The following is a list of valid names for keyboard keys, followed by a short
description of the key.

- insert: Insert
- delete: Delete
- home: Home
- end: End
- page\_up: Page Up
- page\_down: Page Down
- up: Up
- left: Left
- down: Down
- right: Right
- kp\_home: Keypad Home
- kp\_up: Keypad Up
- kp\_page\_up: Keypad Page Up
- kp\_page\_down: Keypad Page Down
- kp\_left: Keypad Left
- kp\_center: Keypad Center
- kp\_right: Keypad Right
- kp\_end: Keypad End
- kp\_down: Keypad Down
- kp\_insert: Keypad Insert
- kp\_delete: Keypad Delete
- kp\_enter: Keypad enter
- kp\_div: Keypad /
- kp\_mul: Keypad *
- kp\_minus: Keypad -
- kp\_plus: Keypad +
- tab: Tab
- backspace: Backspace
- f_num_: Function key _num_

t3keyc
------

<tt>t3keyc</tt>, or the t3key compiler, is a small program to validate keymaps
and to create symbolic links based on the aka directives. Typical usage is:

	t3keyc --link <file>

t3learnkeys
-----------

<tt>t3learnkeys</tt> is a program which can be used to create new keymaps. When
run, it will ask the user to press the different keys to detect the sequences
produced by the terminal. They will be written to a file named after the value
of the TERM environment variable.

The first question which is asked is if there are extra modes to include. This
can be used if the terminal provides alternate keyboard modes which are not
included by default. The default is to record for normal mode (a.k.a. nokx) and
application keypad mode (a.k.a. kx). The latter will be skipped if there is no
smkx terminfo entry (can be verified by running <tt>infocmp</tt>).

Some key combinations don't produce any result, which can be signalled to
<tt>t3learnkeys</tt> by pressing the spacebar instead. If a wrong key is pressed
by mistake, press control-R to redo the input from the last modifier change. To
quit before the learning process is done, press control-C.

WARNING: the program will also ask for combinations like control-alt-delete,
which may cause your machine to reboot. If you are unsure about the results the
key combination may have, simply press spacebar to skip the entry.

For terminal emulators running in the X11 environment, there is an auto-learn
mode activated by <tt>--auto-learn</tt>, which will emulate pressing the keys.
You may have to configure the terminal emulator to accept "SendKey" events for
this to work.
