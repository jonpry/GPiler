%{
/* 
GPiler - parser.y
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

#include "node.h"
#include <cstdio>
#include <cstdlib>
NBlock *programBlock; /* the top level root node of our final AST */

extern int yylex();
extern int yylineno;
extern char* yytext;
void yyerror(const char *s) { std::printf("Error: %d: %s at %s\n", yylineno,s,yytext);std::exit(1); }
%}

/* Represents the many different ways we can access our data */
%union {
	Node *node;
	NBlock *block;
	NIdentifier *ident;
	NType *type;
	NMap *map;
	NVariableDeclaration *var_decl;
	std::list<NVariableDeclaration*> *varvec;
	std::list<NMap*> *pipevec;
	std::list<Node*> *nodevec;
	std::list<NIdentifier*> *idvec;
	std::string *string;
	int token;
}

/* Define our terminal symbols (tokens). This should
   match our tokens.l lex file. We also define the node type
   they represent.
 */
%token <string> TIDENTIFIER TINTEGER TDOUBLE
%token <token> TCEQ TCNE TCLT TCLE TCGT TCGE TEQUAL
%token <token> TLPAREN TRPAREN TLBRACE TRBRACE TCOMMA TDOT
%token <token> TPLUS TMINUS TMUL TDIV 
%token <token> TSEMI TLBRACK TRBRACK TCOLON TDCOLON TQUEST
%token <token> TLSL TLSR TAND TOR

/* Define the type of node our nonterminal symbols represent.
   The types refer to the %union declaration above. Ex: when
   we call an ident (defined by union type ident) we are really
   calling an (NIdentifier*). It makes the compiler happy.
 */
%type <ident> ident 

%type <type> type
%type <node> numeric expr assignment func_call pipeline stmt var_decl func_decl_arg func_decl_ret
%type <varvec> func_decl_args func_decl_rets
%type <nodevec> expr_vec
%type <pipevec> pipeline_chain
%type <idvec> id_vec
%type <block> program stmts block func_decls
%type <node> func_decl 
%type <map> map

/* Operator precedence for mathematical operators */
%left TCOLON TQUEST
%left TPLUS TMINUS
%left TCEQ TCNE TCLT TCGT TCLE TCGE
%left TMUL TDIV 
%left TLSL TLSR TAND TOR

%start program

%%

program : func_decls { programBlock = $1; }
	;

func_decls : func_decl { $$ = new NBlock(); $$->children.push_back($1); }
	| func_decls func_decl { $1->children.push_back($2); }
	;

stmts : stmt { $$ = new NBlock(); $$->children.push_back($1); }
	| stmts stmt { $1->children.push_back($2); }
	;

stmt : var_decl TSEMI
	| assignment TSEMI 
	| func_call TSEMI 
	| pipeline TSEMI 
     	;

pipeline : id_vec TDCOLON pipeline_chain TCGT id_vec {$$ = new NPipeLine($1,$5,$3); }
	;

pipeline_chain: map { $$ = new MapList(); $$->push_back($1); } 
	| pipeline_chain TDCOLON map { $1->push_back($3); }
	;

id_vec: ident { $$ = new IdList(); $$->push_back($1); }
	| id_vec TCOMMA ident { $1->push_back($3); }
	;
	
map : ident TLPAREN id_vec TCOLON expr_vec TRPAREN { $$ = new NMap($1,$3,$5); }
	;

block : TLBRACE stmts TRBRACE { $$ = $2; }
	| TLBRACE TRBRACE { $$ = new NBlock(); }
	;

var_decl : type ident { $$ = new NVariableDeclaration(new TypeList{$1}, $2); }
	| type ident TEQUAL expr { $$ = new NVariableDeclaration(new TypeList{$1}, $2, $4); }
	;

func_decl_arg : type ident { $$ = new NVariableDeclaration(new TypeList{$1}, $2); }
	;

func_decl_ret : type ident { $$ = new NVariableDeclaration(new TypeList{$1}, $2); }
	| type  { $$ = new NVariableDeclaration(new TypeList{$1}, 0); }
	;
 
func_decl : func_decl_rets TCOLON ident TLPAREN func_decl_args TRPAREN block
	{ $$ = new NFunctionDeclaration($1, $3, $5, $7); }
	;

type : TIDENTIFIER { $$ = new NType(*$1,0); delete $1; }
	| TLBRACK TIDENTIFIER TRBRACK { $$ = new NType(*$2,1); delete $2; }
	;

func_decl_args : /*blank*/ { $$ = new VariableList(); }
	| func_decl_arg { $$ = new VariableList(); $$->push_back($<var_decl>1); }
	| func_decl_args TCOMMA func_decl_arg { $1->push_back($<var_decl>3); }
	;

func_decl_rets : /*blank*/ { $$ = new VariableList(); }
	| func_decl_ret { $$ = new VariableList(); $$->push_back($<var_decl>1); }
	| func_decl_rets TCOMMA func_decl_ret { $1->push_back($<var_decl>3); }
	;

ident : TIDENTIFIER { $$ = new NIdentifier(*$1); delete $1; }
	;

numeric : TINTEGER { $$ = new NInteger(atol($1->c_str())); delete $1; }
	| TDOUBLE { $$ = new NDouble(atof($1->c_str())); delete $1; }
	;

assignment : id_vec TEQUAL expr { $$ = new NAssignment($1, $3); }

expr :  ident { $<ident>$ = $1; }
	| numeric
  	| expr TCEQ expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TCNE expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TCGT expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TCLT expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TCLE expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TCGE expr { $$ = new NBinaryOperator($1, $2, $3); }
	| expr TPLUS expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TMINUS expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TMUL expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TDIV expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TOR expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TLSL expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TLSR expr { $$ = new NBinaryOperator($1, $2, $3); }
  	| expr TAND expr { $$ = new NBinaryOperator($1, $2, $3); }
	| expr TQUEST expr TCOLON expr { $$ = new NSelect($1, $3, $5); }
     	| TLPAREN expr TRPAREN { $$ = $2; }
	| func_call
	;

func_call : ident TLPAREN expr_vec TRPAREN { $$ = new NMethodCall($1, $3); }
	;

expr_vec : /*blank*/ { $$ = new NodeList(); }
	| expr { $$ = new NodeList(); $$->push_back($1); }
	| expr_vec TCOMMA expr { $1->push_back($3); }
	;

%%
