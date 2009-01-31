%token IDENTIFIER, KEY_MOD, ENTER, LEAVE, INCLUDE, BEST;


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
