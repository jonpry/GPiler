/* 
GPiler - main.cpp
Copyright (C) 2013 Jon Pry and Charles Cooper

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

%{
#include <string>
#include "node.h"
#include "parser.hpp"
#define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng)
#define TOKEN(t) (yylval.token = t)
%}
%x COMMENT
%option yylineno
%option noyywrap
%option nounput
%%

<INITIAL>{
     "(*"              BEGIN(COMMENT);
}
<COMMENT>{
     "*)"      BEGIN(INITIAL);
     [^*\n]+   // eat comment in chunks
     "*"       // eat the lone star
     \n        yylineno++;
}

[ \t] ;
[\n] yylineno++;
[a-zA-Z_][a-zA-Z0-9_]* SAVE_TOKEN; return TIDENTIFIER;
[0-9]+\.[0-9]* SAVE_TOKEN; return TDOUBLE;
[0-9]+ SAVE_TOKEN; return TINTEGER;
"=" return TOKEN(TEQUAL);
"==" return TOKEN(TCEQ);
"!=" return TOKEN(TCNE);
"<" return TOKEN(TCLT);
"<=" return TOKEN(TCLE);
">" return TOKEN(TCGT);
">=" return TOKEN(TCGE);
"(" return TOKEN(TLPAREN);
")" return TOKEN(TRPAREN);
"{" return TOKEN(TLBRACE);
"}" return TOKEN(TRBRACE);
"." return TOKEN(TDOT);
"," return TOKEN(TCOMMA);
"+" return TOKEN(TPLUS);
"-" return TOKEN(TMINUS);
"*" return TOKEN(TMUL);
"/" return TOKEN(TDIV);
";" return TOKEN(TSEMI);
"[" return TOKEN(TLBRACK);
"]" return TOKEN(TRBRACK);
":" return TOKEN(TCOLON);
"::" return TOKEN(TDCOLON);
"?" return TOKEN(TQUEST);
">>" return TOKEN(TLSR);
"<<" return TOKEN(TLSL);
"&" return TOKEN(TAND);
"|" return TOKEN(TOR);
. printf("Unknown token!\n"); yyterminate();

%%
