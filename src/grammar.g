%options "generate-lexer-wrapper";

{
void LLmessage(int class) {
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
