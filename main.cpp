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



#include <iostream>
#include <cstdio>
#include "codegen.h"
#include "node.h"
#include "runtime.h"

using namespace std;

int Node::sTabs = 0;

extern int yyparse();
extern FILE* yyin;
extern NBlock* programBlock;

void createCoreFunctions(CodeGenContext& context);
void compile(Module &mod);


void rewrite_arrays(NFunctionDeclaration *decl, Runtime *runtime){
	if(decl->type->isArray){
		NType *old_type = decl->type;
		decl->SetType(new NType("void",0));

		NVariableDeclaration *var = new NVariableDeclaration(old_type,new NIdentifier("return"),0);
		decl->InsertArgument(decl->arguments->begin(),var);
	}
	runtime->AddFunction(decl);

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

static NType* typeOf(string name, NBlock* pb){
	NodeList::iterator it;
	for(it = pb->children.begin(); it!=pb->children.end(); it++){
		NFunctionDeclaration *func = dynamic_cast<NFunctionDeclaration*> (*it);
		if(func) {
			if(name.compare(func->id->name)==0){
				return func->type;
			}
		}
	}
	return 0;
}

NFunctionDeclaration *extract_func(NBlock* pb, NMap* map,NType* type) {
	VariableList *var_list = new VariableList;
	// turn ids into vars
	for (IdList::iterator it = map->vars->begin(); it != map->vars->end(); it++) {
		var_list->push_back(new NVariableDeclaration(type, *it));
	}
	NBlock *func_block = new NBlock();

	NAssignment *assignment = new NAssignment(*(map->vars->begin()), map->expr);
	assignment->isReturn = 1;
	func_block->add_child(assignment);
	//func_block->expressions.push_back(new NVariableDeclaration(new NType("return",0), *(map->vars->begin())));

	std::string anon_name = create_anon_name();
	NFunctionDeclaration *anon_func = new NFunctionDeclaration(type,
			new NIdentifier(anon_name),
			var_list,
			func_block
			);
	map->anon_name = anon_name;

	///////////////////////////
	GType foo = GetType(pb,map->expr);
	anon_func->SetType(foo.toNode());
	//////////////////////////

	return anon_func;
}

void rewrite_maps(NBlock *pb) {
	for(NodeList::iterator it = pb->children.begin(); it != pb->children.end(); it++) {
		NFunctionDeclaration *func = dynamic_cast<NFunctionDeclaration*> (*it);
		if(func) {
			for (NodeList::iterator it2 = func->block->children.begin(); it2 != func->block->children.end(); it2++) {
				NPipeLine *pipeline = dynamic_cast<NPipeLine*> (*it2);
				if (pipeline) {
					NType* type = typeOf(func,pipeline->src);
					for(MapList::iterator it3 = pipeline->chain->begin(); it3 != pipeline->chain->end(); it3++){
						NFunctionDeclaration *anon_func = extract_func(pb,*it3, type);
						type = anon_func->type;
						pb->add_child(pb->children.begin(), anon_func);
					}
				}
			}
		}
	}
}

//Array arguments are converted to pointers
void rewrite_arrays(NBlock* programBlock, Runtime *runtime){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			rewrite_arrays(decl,runtime);
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
					NType* type = typeOf(decl,pipe->src);
					NVariableDeclaration *dec = new NVariableDeclaration(type, new NIdentifier(temp_name),iptr);
					decl->block->children.insert(it2,dec);

					MapList::iterator it3;
					for(it3 = pipe->chain->begin(); it3!= pipe->chain->end(); it3++){
						NMap* map = *it3;
						string new_name = create_temp_name();
					
						ExpressionList *args = new ExpressionList();
						args->push_back(new NIdentifier(temp_name));
						NMethodCall *mc = new NMethodCall(new NIdentifier(map->anon_name),args);
						type = typeOf(map->anon_name,pb);
						NVariableDeclaration *map_dec = new NVariableDeclaration(type,new NIdentifier(new_name), mc);
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
	if(argc!=2){
		cout << "Usage: parser inputfile\n";
		return 0;
	}

	FILE *infile = fopen(argv[1], "r");
	if (!infile) {
		cout << "Can't open: " << argv[1] << endl;
		return 0;
	}
	// set lex to read from it instead of defaulting to STDIN:
	yyin = infile;

	yyparse();

	cout << "Raw:\n";

	Runtime *runtime = new Runtime();

	std::cout << *programBlock << endl;

	rewrite_arrays(programBlock,runtime);

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
	runtime->print();

	return 0;
}

