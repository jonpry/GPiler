#include <iostream>
#include <cstdio>
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

	NVariableDeclaration *idxvar = new NVariableDeclaration(new NType("int32",0),new NIdentifier("idx"),0);
	decl->arguments->insert(decl->arguments->begin(),idxvar);
}

std::string create_anon_name(void) {
	static int idx = 0;
	char buf[15];
	sprintf(buf,"anon%d",idx++);
	return std::string(buf);
}

void rewrite_map(NMap *map) {
}

NFunctionDeclaration *extract_func(NMap* map) {
	/*
	NFunctionDeclaration *anon_func = new NFunctionDeclaration(new NType("int32",0),
			new NIdentifier(create_anon_name()),
			map->vars,
			map->expr
			);
	return anon_func;
	*/
}

std::vector<NMap*> extract_maps(ExpressionList &el) {
	std::vector<NMap*> ret;
	for (ExpressionList::iterator it = el.begin(); it != el.end(); it++) {
		NPipeLine *pipeline = dynamic_cast<NPipeLine*> (*it);
		if (pipeline) {
			ret.insert(ret.end(), pipeline->chain->begin(), pipeline->chain->end());
		}
	}
	return ret;
}

// walk ast and get all maps
std::vector<NMap*> extract_maps(StatementList &sl) {
	std::vector<NMap*> ret;
	for(StatementList::iterator it = sl.begin(); it != sl.end(); it++) {
		NFunctionDeclaration *func = dynamic_cast<NFunctionDeclaration*> (*it);
		if(func) {
			std::vector<NMap*> maps = extract_maps(func->block->statements);
			ret.insert(ret.end(), maps.begin(), maps.end());
		}
	}
	return ret;
}

std::vector<NMap*> extract_maps(NBlock *pb) {
	return extract_maps(pb->statements);
}

void rewrite_maps(NBlock *pb) {
	std::vector<NMap*> maps = extract_maps(pb);
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

	rewrite_maps(programBlock);
	cout << "Pass2:\n";
	std::cout << *programBlock << endl;

#if 1	
	CodeGenContext context;
	createCoreFunctions(context);
	context.generateCode(*programBlock);
	compile(*context.module);
#endif
	return 0;
}

