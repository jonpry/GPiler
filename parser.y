%{
#include "node.h"
#include <cstdio>
#include <cstdlib>
NBlock *programBlock; /* the top level root node of our final AST */

extern int yylex();
void yyerror(const char *s) { std::printf("Error: %s\n", s);std::exit(1); }
%}

/* Represents the many different ways we can access our data */
%union {
	Node *node;
	NBlock *block;
	NExpression *expr;
	NStatement *stmt;
	NIdentifier *ident;
	NType *type;
	NVariableDeclaration *var_decl;
	std::vector<NVariableDeclaration*> *varvec;
	std::vector<NExpression*> *exprvec;
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
%token <token> TSEMI TLBRACK TRBRACK


/* Define the type of node our nonterminal symbols represent.
   The types refer to the %union declaration above. Ex: when
   we call an ident (defined by union type ident) we are really
   calling an (NIdentifier*). It makes the compiler happy.
 */
%type <ident> ident 
%type <type> type
%type <expr> numeric expr assignment func_call
%type <varvec> func_decl_args
%type <exprvec> call_args
%type <block> program stmts block func_decls
%type <stmt> stmt var_decl func_decl func_decl_arg 

/* Operator precedence for mathematical operators */
%left TPLUS TMINUS
%left TCEQ TCNE TCLT TCGT TCLE TCGE
%left TMUL TDIV

%start program

%%

program : func_decls { programBlock = $1; }
	;

func_decls : func_decl { $$ = new NBlock(); $$->statements.push_back($<stmt>1); }
	| func_decls func_decl { $1->statements.push_back($<stmt>2); }
	;

stmts : stmt { $$ = new NBlock(); $$->statements.push_back($<stmt>1); }
	| stmts stmt { $1->statements.push_back($<stmt>2); }
	;

stmt : var_decl TSEMI
	| assignment TSEMI { $$ = new NExpressionStatement(*$1); }
	| func_call TSEMI { $$ = new NExpressionStatement(*$1); }
     	;

block : TLBRACE stmts TRBRACE { $$ = $2; }
	| TLBRACE TRBRACE { $$ = new NBlock(); }
	;

var_decl : type ident { $$ = new NVariableDeclaration(*$1, *$2); }
	| type ident TEQUAL expr { $$ = new NVariableDeclaration(*$1, *$2, $4); }
	;

func_decl_arg : type ident { $$ = new NVariableDeclaration(*$1, *$2); }
	;
 
func_decl : type ident TLPAREN func_decl_args TRPAREN block
	{ $$ = new NFunctionDeclaration(*$1, *$2, *$4, *$6); delete $4; }
	;

type : TIDENTIFIER { $$ = new NType(*$1,0); delete $1; }
	| TLBRACK TIDENTIFIER TRBRACK { $$ = new NType(*$2,1); delete $2; }
	;

func_decl_args : /*blank*/ { $$ = new VariableList(); }
	| func_decl_arg { $$ = new VariableList(); $$->push_back($<var_decl>1); }
	| func_decl_args TCOMMA func_decl_arg { $1->push_back($<var_decl>3); }
	;

ident : TIDENTIFIER { $$ = new NIdentifier(*$1); delete $1; }
	;

numeric : TINTEGER { $$ = new NInteger(atol($1->c_str())); delete $1; }
	| TDOUBLE { $$ = new NDouble(atof($1->c_str())); delete $1; }
	;

assignment : ident TEQUAL expr { $$ = new NAssignment(*$<ident>1, *$3); }

expr :  ident { $<ident>$ = $1; }
	| numeric
  	| expr TCEQ expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
  	| expr TCNE expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
  	| expr TCGT expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
  	| expr TCLT expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
  	| expr TCLE expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
  	| expr TCGE expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
	| expr TPLUS expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
  	| expr TMINUS expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
  	| expr TMUL expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
  	| expr TDIV expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
     	| TLPAREN expr TRPAREN { $$ = $2; }
	| func_call
	;

func_call : ident TLPAREN call_args TRPAREN { $$ = new NMethodCall(*$1, *$3); delete $3; };

call_args : /*blank*/ { $$ = new ExpressionList(); }
	| expr { $$ = new ExpressionList(); $$->push_back($1); }
	| call_args TCOMMA expr { $1->push_back($3); }
	;

%%
