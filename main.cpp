#include <iostream>
#include "codegen.h"
#include "node.h"

using namespace std;

int Node::sTabs = 0;

extern int yyparse();
extern NBlock* programBlock;

void createCoreFunctions(CodeGenContext& context);
void compile(Module &mod);

int main(int argc, char **argv)
{
	yyparse();
	std::cout << *programBlock << endl;

//	CodeGenContext context;
//	createCoreFunctions(context);
//	context.generateCode(*programBlock);
//	compile(*context.module);

	return 0;
}
