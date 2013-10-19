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
		decl->SetType(new NType("void",0));

		NVariableDeclaration *var = new NVariableDeclaration(old_type,new NIdentifier("return"),0);
		decl->InsertArgument(decl->arguments->begin(),var);
	}

	NVariableDeclaration *idxvar = new NVariableDeclaration(new NType("int32",0),new NIdentifier("idx"),0);
	decl->InsertArgument(decl->arguments->begin(),idxvar);
}

std::string create_anon_name(void) {
	static unsigned idx = 0;
	char buf[16];
	sprintf(buf,"anon%u",idx++);
	return std::string(buf);
}

std::string create_temp_name(void) {
	static unsigned idx = 0;
	char buf[32];
	sprintf(buf,"pipeline.temp%u",idx++);
	return std::string(buf);
}

static NType* typeOf(NFunctionDeclaration *decl, NIdentifier *var){
	for(VariableList::iterator it = decl->arguments->begin(); it != decl->arguments->end(); it++){
		NVariableDeclaration *vdec = *it;
		if(vdec->id->name.compare(var->name) == 0){
			return new NType(vdec->type->name,0);
		}
	}
	//TODO: search function block for declarations too
	cout << "Couldn't find var\n";
	return 0;
}

NFunctionDeclaration *extract_func(NMap* map,NType* type) {
	VariableList *var_list = new VariableList;
	// turn ids into vars
	for (IdList::iterator it = map->vars->begin(); it != map->vars->end(); it++) {
		var_list->push_back(new NVariableDeclaration(type, *it));
	}
	NBlock *func_block = new NBlock();

	NAssignment *assignment = new NAssignment(*(map->vars->begin()), map->expr);
	assignment->isReturn = 1;
	func_block->children.push_back(assignment);
	//func_block->expressions.push_back(new NVariableDeclaration(new NType("return",0), *(map->vars->begin())));

	std::string anon_name = create_anon_name();
	NFunctionDeclaration *anon_func = new NFunctionDeclaration(type,
			new NIdentifier(anon_name),
			var_list,
			func_block
			);
	map->anon_name = anon_name;
	return anon_func;
}

void rewrite_maps(NBlock *pb) {
	for(NodeList::iterator it = pb->children.begin(); it != pb->children.end(); it++) {
		NFunctionDeclaration *func = dynamic_cast<NFunctionDeclaration*> (*it);
		if(func) {
			for (NodeList::iterator it2 = func->block->children.begin(); it2 != func->block->children.end(); it2++) {
				NPipeLine *pipeline = dynamic_cast<NPipeLine*> (*it2);
				if (pipeline) {
					for(MapList::iterator it3 = pipeline->chain->begin(); it3 != pipeline->chain->end(); it3++){
						NFunctionDeclaration *anon_func = extract_func(*it3, typeOf(func,pipeline->src));
						pb->children.insert(pb->children.begin(), anon_func);
					}
				}
			}
		}
	}
}

void rewrite_arrays(NBlock* programBlock){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			rewrite_arrays(decl);
		}
	}
}

void rewrite_pipelines(NBlock *pb){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			NodeList::iterator it2;
			for(it2 = decl->block->children.begin(); it2 != decl->block->children.end(); ){
				NPipeLine *pipe = dynamic_cast<NPipeLine*>(*it2);
				if(pipe){
					string temp_name = create_temp_name();
					//Load it
					NArrayRef *iptr = new NArrayRef(pipe->src,new NIdentifier("idx"));
					NVariableDeclaration *dec = new NVariableDeclaration(typeOf(decl,pipe->src), new NIdentifier(temp_name),iptr);
					decl->block->children.insert(it2,dec);

					MapList::iterator it3;
					for(it3 = pipe->chain->begin(); it3!= pipe->chain->end(); it3++){
						NMap* map = *it3;
						string new_name = create_temp_name();
					
						ExpressionList *args = new ExpressionList();
						args->push_back(new NIdentifier(temp_name));
						NMethodCall *mc = new NMethodCall(new NIdentifier(map->anon_name),args);
						NVariableDeclaration *map_dec = new NVariableDeclaration(typeOf(decl,pipe->src),new NIdentifier(new_name), mc);
						decl->block->children.insert(it2,map_dec);						

						temp_name = new_name;
					}

					//Store it
					NArrayRef *sptr = new NArrayRef(pipe->dest,new NIdentifier("idx"));
					NAssignment *store = new NAssignment(sptr,new NIdentifier(temp_name));
					decl->block->children.insert(it2,store);

					//TODO: delete the pipeline now that it is dangling
					it2 = decl->block->children.erase(it2);
				}else{
					it2++;
				}
			}
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

	cout << "Pass3:\n";
	rewrite_pipelines(programBlock);
	cout << *programBlock;

#if 1
	CodeGenContext context;
//	createCoreFunctions(context);
	context.generateCode(*programBlock);
	compile(*context.module);
#endif
	return 0;
}

