#include <iostream>
#include "codegen.h"
#include "node.h"

using namespace std;

int Node::sTabs = 0;

extern int yyparse();
extern NBlock* programBlock;

void createCoreFunctions(CodeGenContext& context);
void compile(Module &mod);


void rewrite_arrays(NFunctionDeclaration *decl){
	if(decl->type->isArray){
		NType *old_type = decl->type;
		decl->type = new NType("void",0);

		NVariableDeclaration *var = new NVariableDeclaration(old_type,new NIdentifier("return"),0);
		decl->arguments->insert(decl->arguments->begin(),var);
	}
}

void rewrite_arrays(NBlock* programBlock){
	StatementList::iterator it;
	for(it = programBlock->statements.begin(); it != programBlock->statements.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			rewrite_arrays(decl);
		}
	}
}


int main(int argc, char **argv)
{
	yyparse();
	
	cout << "Raw:\n";

	std::cout << *programBlock << endl;

	rewrite_arrays(programBlock);

	cout << "Pass1:\n";

	std::cout << *programBlock << endl;

#if 1	
	CodeGenContext context;
	createCoreFunctions(context);
	context.generateCode(*programBlock);
	compile(*context.module);
#endif
	return 0;
}
