%options "generate-lexer-wrapper generate-symbol-table lowercase-symbols";

{

#include "ckeyc.h"

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

CKeyMap **current_map = &maps;
CKeyNode **current_node;
}

%token STRING;
%label IDENTIFIER, "key";
%label KEY_MOD, "key";
%label ENTER, "%enter";
%label LEAVE, "%leave";
%label INCLUDE, "%include";
%label BEST, "%best";
%label MISSING_KEY, "key";
%start parse, description;

description :
	[
		map
	|
		BEST '='? IDENTIFIER
		{
			best = safe_strdup(yytext);
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
