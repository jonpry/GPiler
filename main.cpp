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
	decl->returns->reverse();
	for(it = decl->returns->begin(); it!=decl->returns->end();){
		NVariableDeclaration *vdec = *it;
		if(!(*vdec->types->begin())->isArray){
			(*vdec->types->begin())->isPointer = 1;
		}
		decl->InsertArgument(decl->arguments->begin(),vdec);
		it = decl->returns->erase(it);
	}

	VariableList vlist;
	decl->SetType(vlist);	
     
	if(!decl->isScalar()){
		NVariableDeclaration *idxvar = new NVariableDeclaration(new TypeList{new NType("int32",0)},new NIdentifier("idx"),0);
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

//TODO: this is all wrong, doesn't support multiple parameters
NFunctionDeclaration *extract_func(NBlock* pb, NMap* map,TypeList* types) {
	VariableList *var_list = new VariableList;
	// turn ids into vars
	{
		IdList::iterator it;
		TypeList::iterator it2;
		for (it = map->vars->begin(), it2 = types->begin(); it != map->vars->end() && it2 != types->end(); it++, it2++) {
			var_list->push_back(new NVariableDeclaration(new TypeList{*it2}, *it));
		}
		if(it!=map->vars->end() || it2 != types->end()){
			cout << "Map argument count mismatch\n";
			exit(-1);
		}
	}	
	NBlock *func_block = new NBlock();
	VariableList *retlist = new VariableList();

	{
		NodeList::iterator it;
		int count=0;
		for(it=map->exprs->begin(); it!= map->exprs->end(); it++){
			char ret_name[64];
			sprintf(ret_name,"return.%d",count++);
			NIdentifier *id = new NIdentifier(ret_name);
			//Don't set type for now, do it after the function is setup
			NVariableDeclaration *vardec = new NVariableDeclaration(0,(NIdentifier*)id->clone());
			retlist->push_back(vardec);

			NAssignment *assignment = new NAssignment(new IdList{id}, *it);
			func_block->add_child(assignment);
		}
	}
	std::string anon_name = create_anon_name();
	NFunctionDeclaration *anon_func = new NFunctionDeclaration(
			retlist,
			new NIdentifier(anon_name),
			var_list,
			func_block
			);
	anon_func->isGenerated=1;
	map->anon_name = anon_name;

	///////////////////////////
	{
		VariableList::iterator it;	
		NodeList::iterator it2;
		for(it=anon_func->returns->begin(), it2=anon_func->block->children.begin(); it!=anon_func->returns->end(); it++, it2++){
			Node *rhs = ((NAssignment*)*it2)->rhs;
			GTypeList foo = GetType(pb,rhs);
			NType* type = (*foo.begin()).toNode();
			type->isArray=0;
			(*it)->SetType(type);
		}
	}
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
/*
void generate_runtime(NBlock* programBlock, Runtime* runtime){
	NodeList::iterator it;
	for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl){
			if(!decl->isGenerated)
			runtime->AddFunction(decl);
		}
	}
}*/

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
					for(TypeList::iterator it3=vdec->types->begin(); it3!=vdec->types->end(); it3++){
						(*it3)->isArray=0;
					}
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

IdList *copyIdList(IdList* src){
	IdList *ret = new IdList();
	for(IdList::iterator it=src->begin(); it!=src->end(); it++){
		ret->push_back((NIdentifier*)(*it)->clone());
	}
	return ret;
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
					TypeList *types = new TypeList(typeOf(decl,*pipe->src,0));
					NVariableDeclaration *dec = new NVariableDeclaration(types, new NIdentifier(temp_name),new NZip(copyIdList(pipe->src)));
					decl->block->children.insert(it2,dec);

					MapList::iterator it3;
					for(it3 = pipe->chain->begin(); it3!= pipe->chain->end(); it3++){
						NMap* map = *it3;
						string new_name = create_temp_name();

						{
							NFunctionDeclaration *anon_func = extract_func(pb,*it3, types);
							//TODO: this kind of conditionally deals with types as a special case
							//for filter, however it should really be done through multiple argument support
							if(!isPredicate(*it3))
								types = new TypeList(ntypesOf(((Node*)anon_func)->GetType(),0));
							pb->add_child(pb->children.begin(), anon_func);
						}
						
						map->SetInput(new NIdentifier(temp_name));
						NVariableDeclaration *map_dec = new NVariableDeclaration(types,new NIdentifier(new_name), map);
						decl->block->children.insert(it2,map_dec);						

						temp_name = new_name;
					}

					Node* store = new NAssignment(pipe->dest,new NIdentifier(temp_name));
					decl->block->add_child(it2,store);

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
				if(assn){
					if(assn->lhs){
						NType* type = *typeOf(decl,*assn->lhs->begin(),1).begin();
						if(type->isArray){
							assn->SetArray(new NArrayRef(*assn->lhs->begin(),new NIdentifier("idx")));
						}
					}
					NIdentifier* id= dynamic_cast<NIdentifier*>(assn->rhs);
					if(id){
						cout << id->name << "\n";
						NType* type = *typeOf(decl,id,1).begin();
						if(type->isArray){
							assn->SetExpr(new NArrayRef(id,new NIdentifier("idx")));
						}
					}
				}
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
			map<string,TypeList*> types;
			for(NodeList::iterator it2 = decl->block->children.begin(); it2 != decl->block->children.end(); it2++){

				IdList ids;
				(*it2)->GetIdRefs(ids);

				//If rhs makes a reference rename it
				for(IdList::iterator it3=ids.begin(); it3!=ids.end(); it3++){
					NIdentifier *id = *it3;
					if(vars.find(id->name) != vars.end() && vars[id->name] > 0){
						char new_name[64];
						sprintf(new_name,"%s.%d", id->name.c_str(),  vars[id->name]);
						id->name = new_name; 					
					}
				}
				NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it2);
				if(vdec){
					vars[vdec->id->name] = -1; //One assignment for free
					types[vdec->id->name] = vdec->types;
					if(vdec->assignmentExpr){
						NAssignment* new_assn = new NAssignment(new IdList{(NIdentifier*)vdec->id->clone()}, vdec->assignmentExpr);
						vdec->SetExpr(0);

						//TODO: better insert after
						it2++;
						decl->block->add_child(it2,new_assn);
						it2--;
					}
				}

				NAssignment* assn = dynamic_cast<NAssignment*>(*it2);
				if(assn){
					for(IdList::iterator it3 = assn->lhs->begin(); it3 != assn->lhs->end(); it3++){
						if(vars.find((*it3)->name) != vars.end()){
							//rename!!!!!!!
							vars[(*it3)->name] = vars[(*it3)->name] + 1;
							if(vars[(*it3)->name] < 1)
								continue;
							char new_name[64];	
							sprintf(new_name,"%s.%d", (*it3)->name.c_str(),  vars[(*it3)->name]);

							//Create new vdec
							NVariableDeclaration *new_dec = new NVariableDeclaration(types[(*it3)->name], new NIdentifier(new_name), 0);
							//insert before
							decl->block->children.insert(it2,new_dec);
							//it2++;

							(*it3)->name = new_name; 
						}
					}
				}
			}
		}
	}
}

void map_to_args(NIdentifier *dest, NodeList *list, NMap *map){
	if(map->exprs->size() == 1){
		list->push_back(new NRef(dest));
	}else{
		int count = 0;
		for(NodeList::iterator it = map->exprs->begin(); it!=map->exprs->end(); it++){
			char new_name[64];
			sprintf(new_name, "%s.%d", dest->name.c_str(), count++);
			list->push_back(new NRef(new NIdentifier(new_name)));
		}
	}

	if(map->vars->size() == 1){
		list->push_back(map->input);
	}else{
		int count = 0;
		for(IdList::iterator it = map->vars->begin(); it!= map->vars->end(); it++){
			char new_name[64];
			sprintf(new_name, "%s.%d", map->input->name.c_str(), count++);
			list->push_back(new NIdentifier(new_name));
		}
	}
}

void rewrite_triads(NBlock *pb){
        NodeList::iterator it;
        for(it = programBlock->children.begin(); it != programBlock->children.end(); it++){
                NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
                if(decl){
                        NodeList::iterator it2;
                        for(it2 = decl->block->children.begin(); it2 != decl->block->children.end();){
                                NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it2);
                                if(vdec){
					if(vdec->types->size() > 1){
						int count=0;
						for(TypeList::iterator it3=vdec->types->begin(); it3!=vdec->types->end(); it3++){
							char new_name[64];
							sprintf(new_name,"%s.%d",vdec->id->name.c_str(),count++);
							NVariableDeclaration *new_dec = new NVariableDeclaration(new TypeList{*it3},new NIdentifier(new_name),0);
							decl->block->add_child(it2,new_dec);
						}
						it2 = decl->block->children.erase(it2);
						continue;
					}
				}

				NAssignment *assn = dynamic_cast<NAssignment*>(*it2);
				if(assn){
					NMap* map = dynamic_cast<NMap*>(assn->rhs);
					if(map){
                                                NodeList *args = new NodeList();
						map_to_args(*assn->lhs->begin(),args,map);

						NMethodCall *mc = new NMethodCall(new NIdentifier(map->anon_name),args);
						decl->block->add_child(it2,mc);

						it2 = decl->block->children.erase(it2);
						continue;
					}

					NZip* zip = dynamic_cast<NZip*>(assn->rhs);
					if(zip){
						if(zip->src->size() == 1){
							assn->SetExpr(*zip->src->begin());
						}else{
							int count=0;
							for(IdList::iterator it3 = zip->src->begin(); it3!= zip->src->end(); it3++){
								char new_name[64];
								sprintf(new_name,"%s.%d",(*assn->lhs->begin())->name.c_str(),count++);

								NAssignment *new_assn = new NAssignment(new IdList{new NIdentifier(new_name)}, *it3);
								decl->block->add_child(it2,new_assn);
							}
							it2 = decl->block->children.erase(it2);
							continue;
						}
					}

					NIdentifier *id = dynamic_cast<NIdentifier*>(assn->rhs);
					if(id && assn->lhs->size() > 1){
						int count=0;
						for(IdList::iterator it3 = assn->lhs->begin(); it3!= assn->lhs->end(); it3++){
							char new_name[64];
							sprintf(new_name,"%s.%d",id->name.c_str(),count++);
							NAssignment *new_assn = new NAssignment(new IdList{*it3},new NIdentifier(new_name));
							decl->block->add_child(it2,new_assn);
						}
						it2 = decl->block->children.erase(it2);
						continue;
					}
				}
				it2++;
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
//	Runtime *runtime = new Runtime();

	cout << "Raw:\n";
	std::cout << *programBlock << endl;

	auto_name_returns(programBlock);
	cout << "Pass1:\n";
	std::cout << *programBlock << endl;

	rewrite_pipelines(programBlock);
	cout << "Pass3:\n";
	cout << *programBlock;

//TODO: it's impossible to guarantee all declaration have assignment with zip shits
//	remove_empty_decls(programBlock);
//	cout << "Pass4:\n";
//	cout << *programBlock;

	to_ssa(programBlock);
	cout << "Pass5:\n";
	cout << *programBlock;

//	split_unnatural(programBlock);
	cout << "Pass6:\n";
//	cout << *programBlock;

//TODO: change this pass
	remove_array_temps(programBlock);
	cout << "Pass2:\n";
	std::cout << *programBlock << endl;

	rewrite_triads(programBlock);
	cout << "Pass7:\n";
	cout << *programBlock;

	rewrite_argument_access(programBlock);
	cout << "Pass8:\n";
	cout << *programBlock;

	/////////////////////////////////////////////////////////
	// Passes below this point start losing too much context for building the runtime
//	generate_runtime(programBlock,runtime);

	rewrite_arrays(programBlock);
	cout << "Pass9:\n";
	cout << *programBlock;

#if 1
	CodeGenContext context;
//	createCoreFunctions(context);
	context.generateCode(*programBlock);
	compile(*context.module);
#endif
//	runtime->print();

	return 0;
}

