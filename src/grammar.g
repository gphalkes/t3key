%options "generate-lexer-wrapper generate-symbol-table";

{

#include "ckeyc.h"

void LLmessage(int class) {
	switch (class) {
		case LL_MISSINGEOF:
			fatal("%d: Expected %s, found %s. Skipping.\n", line_number, LLgetSymbol(EOFILE), LLgetSymbol(LLsymb));
			break;
		case LL_DELETE:
			fatal("%d: Skipping unexpected %s.\n", line_number, LLgetSymbol(LLsymb));
			break;
		default:
			fatal("%d: Expected %s, found %s. Inserting.\n", line_number, LLgetSymbol(class), LLgetSymbol(LLsymb));
			break;
	}
}

}

%token IDENTIFIER, KEY_MOD, ENTER, LEAVE, INCLUDE, BEST, STRING;
%start parse, description;

description :
	[
		map
	|
		BEST IDENTIFIER
	]+
;

map :
	IDENTIFIER '{'
	[
		key
	|
		INCLUDE IDENTIFIER
	|
		[
			ENTER
		|
			LEAVE
		]
		'='
		[
			IDENTIFIER
		|
			STRING
		]
	]+
	'}'
;

key :
	[
		IDENTIFIER
	|
		KEY_MOD
	]
	'='
	[
		STRING
	|
		IDENTIFIER
	|
		KEY_MOD
	]
;
