/* Copyright (C) 2010 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
%options "generate-lexer-wrapper generate-symbol-table lowercase-symbols";

{
#include <stdlib.h>
#include "t3keyc.h"

void LLmessage(int class) {
	switch (class) {
		case LL_MISSINGEOF:
			fatal("%d: Expected %s, found %s.\n", line_number, LLgetSymbol(EOFILE), LLgetSymbol(LLsymb));
			break;
		case LL_DELETE:
			fatal("%d: Unexpected %s.\n", line_number, LLgetSymbol(LLsymb));
			break;
		default:
			fatal("%d: Expected %s, found %s.\n", line_number, LLgetSymbol(class), LLgetSymbol(LLsymb));
			break;
	}
}

t3_key_map_t **current_map = &maps;
t3_key_node_t **current_node;
}

%token STRING;
%label IDENTIFIER, "key";
%label KEY_MOD, "key";
%label ENTER, "%enter";
%label LEAVE, "%leave";
%label INCLUDE, "%include";
%label BEST, "%best";
%label AKA, "%aka";
%label MISSING_KEY, "key";
%start parse, description;

description {
	t3_key_string_list_t **next_aka = &akas;
} :
	[
		map
	|
		BEST '='? IDENTIFIER
		{
			best = safe_strdup(yytext);
		}
	|
		AKA '='?
		[
			IDENTIFIER
		|
			STRING
		]
		{
			if ((*next_aka = malloc(sizeof(t3_key_string_list_t))) == NULL)
				fatal("Out of memory\n");
			(*next_aka)->string = safe_strdup(yytext);
			if (yytext[0] == '"')
				parse_escapes((*next_aka)->string);
			(*next_aka)->next = NULL;
			next_aka = &(*next_aka)->next;
		}
	]+
;

map
{
	char *key;
}
:
	IDENTIFIER
	{
		*current_map = new_map(yytext);
		current_node = &(*current_map)->mapping;
	}
	'{'
	[
		key
	|
		INCLUDE '='? IDENTIFIER
		{
			*current_node = new_node("%include", NULL, yytext);
			current_node = &(*current_node)->next;
		}
	|
		[
			ENTER
		|
			LEAVE
		]
		{
			key = safe_strdup(yytext);
		}
		'='
		[
			IDENTIFIER
			{
				*current_node = new_node(key, NULL, yytext);
			}
		|
			STRING
			{
				*current_node = new_node(key, yytext, NULL);
			}
		]
		{
			current_node = &(*current_node)->next;
		}
	]+
	'}'
	{
		current_map = &(*current_map)->next;
	}
;

key
{
	char *name;
}
:
	[
		IDENTIFIER
	|
		KEY_MOD
	|
		%default MISSING_KEY
	]
	{
		name = safe_strdup(yytext);
	}
	'='
	[
		STRING
/*	|
		IDENTIFIER
	|
		KEY_MOD */
	]
	{
		*current_node = new_node(name, yytext, NULL);
		current_node = &(*current_node)->next;
	}
;
