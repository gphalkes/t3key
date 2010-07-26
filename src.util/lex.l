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
%{
#include "grammar.h"
int line_number = 1;
%}

%option noyywrap

%%
[ \t]
\n                                line_number++;
#.*
[a-zA-Z_][a-zA-Z_0-9]*            return IDENTIFIER;
[a-zA-Z_][a-zA-Z_0-9]*\+[cms]+    return KEY_MOD;
%enter                            return ENTER;
%leave                            return LEAVE;
%include                          return INCLUDE;
%best                             return BEST;
%aka                              return AKA;
\"([^\\"]|\\.)*\"                 return STRING;
[{}=]                             return yytext[0];
