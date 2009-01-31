

%%
[ \t]
\n                                line_number++;
#.*
[a-zA-Z_][a-zA-Z_0-9]             return IDENTIFIER;
[a-zA-Z_][a-zA-Z_0-9]*@[cms]+     return KEY_MOD;
%enter                            return ENTER;
%leave                            return LEAVE;
%include                          return INCLUDE;
%best                             return BEST;
\"([^\\]|\\.)*\"                  return STRING;
[{}=]                             return yytext[0];