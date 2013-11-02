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
void split_unnatural(NBlock *pb);


//This converts all array return types into function arguments. 
//TODO: fuctions with multiple scalar returns must be modified somehow
void rewrite_arrays(NFunctionDeclaration *decl){
	VariableList::iterator it;
//	cout << decl << ", " << decl->returns << "\n";
	for(it = decl->returns->begin(); it!=decl->returns->end();){
		NVariableDeclaration *vdec = *it;
//		cout << vdec << ", " << vdec->type << "\n";
		if(vdec->type->isArray){
			//NType *old_type = vdec->type;
//			decl->SetType(new NType("void",0));	

//			NVariableDeclaration *var = new NVariableDeclaration(old_type,new NIdentifier("return"),0);
			//TODO: assign default names
//			cout << "Insert\n";
			decl->InsertArgument(decl->arguments->begin(),vdec);
//			cout << "Remove: " << decl->returns << "\n";
			it = decl->returns->erase(it);
		}else
			it++;
	}
//	cout << "done\n";
     
	if(!decl->isScalar()){
		NVariableDeclaration *idxvar = new NVariableDeclaration(new NType("int32",0),new NIdentifier("idx"),0);
		decl->InsertArgument(decl->arguments->begin(),idxvar);
	}
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
	VariableList *retlist = new VariableList();
	NVariableDeclaration *vardec = new NVariableDeclaration(type,new NIdentifier("return"));
	retlist->push_back(vardec);
	NFunctionDeclaration *anon_func = new NFunctionDeclaration(
			retlist,
			new NIdentifier(anon_name),
			var_list,
			func_block
			);
	anon_func->isGenerated=1;
	map->anon_name = anon_name;

	///////////////////////////
	GType foo = GetType(pb,map->expr);
	vardec->SetType(foo.toNode());
	//////////////////////////

	return anon_func;
}

//Array arguments are converted to pointers
void rewrite_arrays(NBlock* programBlock){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			rewrite_arrays(decl);
		}
	}
}

void generate_runtime(NBlock* programBlock, Runtime* runtime){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			if(!decl->isGenerated)
			runtime->AddFunction(decl);
		}
	}
}

//Array temps become scalars
void remove_array_temps(NBlock* programBlock){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			NodeList::iterator it2;
			for(it2 = decl->block->children.begin(); it2!=decl->block->children.end(); it2++){
				NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it2);
				if(vdec){
					vdec->type->isArray=0;
				}
			}
		}
	}
}

bool isPredicate(NMap* map){
	if(map->name->name.compare("filter")==0)
		return true;
	return false;
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
					NType *type = typeOf(decl,pipe->src,0);
					NVariableDeclaration *dec = new NVariableDeclaration(type, new NIdentifier(temp_name),pipe->src);
					decl->block->children.insert(it2,dec);

					MapList::iterator it3;
					for(it3 = pipe->chain->begin(); it3!= pipe->chain->end(); it3++){
						NMap* map = *it3;
						string new_name = create_temp_name();

						{
							NFunctionDeclaration *anon_func = extract_func(pb,*it3, type);
							//TODO: this kind of conditionally deals with types as a special case
							//for filter, however it should really be done through multiple argument support
							if(!isPredicate(*it3))
								type = anon_func->returns->front()->type;
							pb->add_child(pb->children.begin(), anon_func);
						}
						
						map->SetInput(new NIdentifier(temp_name));
						NVariableDeclaration *map_dec = new NVariableDeclaration(type,new NIdentifier(new_name), map);
						decl->block->children.insert(it2,map_dec);						

						temp_name = new_name;
					}

					NExpression* store = new NAssignment(pipe->dest,new NIdentifier(temp_name));
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

//Go ahead and fix codes to deal with any pointers that may be present
void rewrite_argument_access(NBlock *pb){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			NodeList::iterator it2;
			for(it2 = decl->block->children.begin(); it2 != decl->block->children.end(); it2++){
				NAssignment *assn = dynamic_cast<NAssignment*>(*it2);
				if(assn && assn->lhs){
					NType* type = typeOf(decl,assn->lhs,1);
					if(type->isArray){
						assn->SetArray(new NArrayRef(assn->lhs,new NIdentifier("idx")));
					}
				}
				NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it2);
				if(vdec && vdec->assignmentExpr &&  dynamic_cast<NIdentifier*>(vdec->assignmentExpr)){
					NType* type = typeOf(decl,(NIdentifier*)vdec->assignmentExpr,1);
					if(type->isArray){
						vdec->SetExpr(new NArrayRef((NIdentifier*)vdec->assignmentExpr,new NIdentifier("idx")));
					}
				}
			}
		}
	}
}

void rewrite_triads(NBlock *pb){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			NodeList::iterator it2;
			for(it2 = decl->block->children.begin(); it2 != decl->block->children.end(); it2++){
				NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it2);
				if(vdec){
					NMap* map = dynamic_cast<NMap*>(vdec->assignmentExpr);
					if(map){
						ExpressionList *args = new ExpressionList();
						args->push_back(map->input);
						NMethodCall *mc = new NMethodCall(new NIdentifier(map->anon_name),args);
						vdec->SetExpr(mc);
					}
				}
			}
		}
	}
}

void remove_empty_decls(NBlock *pb){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl && !decl->isGenerated){
			//Locate all declarations that have no assigment
			for(NodeList::iterator it2 = decl->block->children.begin(); it2 != decl->block->children.end();){
				NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it2);
				if(vdec && !vdec->assignmentExpr){
					//See if we can locate an assigment for it
					//bool found=false;
					for(NodeList::iterator it3 = it2; it3 != decl->block->children.end(); it3++){
						NAssignment *assn = dynamic_cast<NAssignment*>(*it3);
						if(assn && vdec->id->name.compare(assn->lhs->name)==0){
							//found = true;
							vdec->SetExpr(assn->rhs);
							decl->block->children.insert(it3,vdec);
							decl->block->children.erase(it3);
							break;
						}
					}
					//Alway erase the declaration since it has been moved 
					it2 = decl->block->children.erase(it2);
				}else
					it2++;
			}
		}
	}
}

//Assumes all declaration have an expression
void to_ssa(NBlock *pb){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl && !decl->isGenerated){
			map<string,int> vars;
			map<string,NType*> types;
			for(NodeList::iterator it2 = decl->block->children.begin(); it2 != decl->block->children.end(); it2++){
				IdList ids;
				((NExpression*)(*it2))->GetIdRefs(ids);

				for(IdList::iterator it3=ids.begin(); it3!=ids.end(); it3++){
					NIdentifier *id = *it3;
					if(vars.find(id->name) != vars.end() && vars[id->name]){
						char new_name[64];
						sprintf(new_name,"%s.%d", id->name.c_str(),  vars[id->name]);
						id->name = new_name; 					
					}
				}

				NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it2);
				if(vdec){
					vars[vdec->id->name] = 0;
					types[vdec->id->name] = vdec->type;
				}

				NAssignment* assn = dynamic_cast<NAssignment*>(*it2);
				if(assn){
					if(vars.find(assn->lhs->name) != vars.end()){
						//TODO: change assn to vdec
						NVariableDeclaration *new_dec = new NVariableDeclaration(types[assn->lhs->name], assn->lhs, assn->rhs);
						decl->block->children.insert(it2,new_dec);
						it2 = decl->block->children.erase(it2);
						it2--;

						//rename!!!!!!!
						vars[assn->lhs->name] = vars[assn->lhs->name] + 1;
						char new_name[64];
						sprintf(new_name,"%s.%d", assn->lhs->name.c_str(),  vars[assn->lhs->name]);
						assn->lhs->name = new_name; 
					}
				}
			}
		}
	}
}


void auto_name_returns(NBlock *pb){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			VariableList::iterator it2;
			//First count the number of unnamed parameters
			int missing=0;
			for(it2 = decl->returns->begin(); it2!=decl->returns->end(); it2++){
				NVariableDeclaration *vdec = *it2;
				if(!vdec->id){
					missing++;
				}
			}

			int count=0;
			for(it2 = decl->returns->begin(); it2!=decl->returns->end(); it2++){
				NVariableDeclaration *vdec = *it2;
				if(!vdec->id){
					char name[64];
					if(missing>1){
						sprintf(name,"return.%d",count++);
					}else{
						sprintf(name,"return");
					}
					vdec->id = new NIdentifier(name);
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
	Runtime *runtime = new Runtime();

	cout << "Raw:\n";
//	std::cout << *programBlock << endl;

	auto_name_returns(programBlock);
	cout << "Pass1:\n";
//	std::cout << *programBlock << endl;

	remove_array_temps(programBlock);
	cout << "Pass2:\n";
//	std::cout << *programBlock << endl;

	rewrite_pipelines(programBlock);
	cout << "Pass3:\n";
//	cout << *programBlock;

	remove_empty_decls(programBlock);
	cout << "Pass4:\n";
//	cout << *programBlock;

	to_ssa(programBlock);
	cout << "Pass5:\n";
//	cout << *programBlock;

	split_unnatural(programBlock);
	cout << "Pass6:\n";
//	cout << *programBlock;

	rewrite_triads(programBlock);
	cout << "Pass7:\n";
//	cout << *programBlock;

	rewrite_argument_access(programBlock);
	cout << "Pass8:\n";
//	cout << *programBlock;

	/////////////////////////////////////////////////////////
	// Passes below this point start losing too much context for building the runtime
	generate_runtime(programBlock,runtime);

	rewrite_arrays(programBlock);
	cout << "Pass9:\n";
//	cout << *programBlock;

#if 1
	CodeGenContext context;
//	createCoreFunctions(context);
	context.generateCode(*programBlock);
	compile(*context.module);
#endif
	runtime->print();

	return 0;
}

