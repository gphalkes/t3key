/** @mainpage

@section main-intro Introduction

The libt3key library provides functions for retrieving the character sequences
used by terminals to represent keys pressed by the user. Although the terminfo
database provides part of this information, it lacks information for the
sequences returned by modern terminals/terminal emulators for many combinations
of modifiers with other keys. For example, many terminal emulators provide
separate character sequences for Control combined with the cursor keys, which
is not stored in the terminfo database.

Many programs contain their own databases with this information. This library
tries to be a database usable by many programs, such that all programs can
benefit from the information collected.

libt3key is part of the <A HREF="http://os.ghalkes.nl/t3/">Tilde Terminal
Toolkit (T3)</A>.

@section sec_use How to Use

Normal use of the library would consist of calling ::t3_key_load_map with
@c NULL as the @c term and @c map_name arguments, to load the best map. After
loading the map of character sequences, use ::t3_key_get_named_node to retrieve
the desired character sequences. The @c name argument must be one of:

- insert: Insert
- delete: Delete
- home: Home
- end: End
- page_up: Page Up
- page_down: Page Down
- up: Up
- left: Left
- down: Down
- right: Right
- kp_home: Keypad Home
- kp_up: Keypad Up
- kp_page_up: Keypad Page Up
- kp_page_down: Keypad Page Down
- kp_left: Keypad Left
- kp_center: Keypad Center
- kp_right: Keypad Right
- kp_end: Keypad End
- kp_down: Keypad Down
- kp_insert: Keypad Insert
- kp_delete: Keypad Delete
- kp_enter: Keypad enter
- kp_div: Keypad /
- kp_mul: Keypad *
- kp_minus: Keypad -
- kp_plus: Keypad +
- tab: Tab
- backspace: Backspace
- f&lt;num&gt;: Function key &lt;num&gt;

optionally combined with a plus symbol and one or more of the modifier letters
c[ontrol], m[eta] and s[hift] (in alphabetical order). For example, to retrieve
the character sequence for Meta-Shift-right, pass the name @c right+ms.

\note Although the library provides separate character sequences for
the keypad and regular cursor keys, it is a very bad idea to distinghuish
between them. They should be treated as if they are the same keys as far as the
behavior of the program is concerned.

Each key may have multiple character sequences associated with it. By calling
::t3_key_get_named_node multiple times, the second and later times with @c NULL
as the @c name argument @b and the returned value as the first argument, all
character sequences can be retrieved.
*/
