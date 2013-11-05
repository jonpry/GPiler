/* 
GPiler - runtime.cpp
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
#include "codegen.h"
#include "parser.hpp"
#include "runtime.h"

using namespace std;

///////////////////////NEW PLAN/////////////////////////////

//TODO: Maybe move this into function member, taking std::string as argument
bool varPresent(NFunctionDeclaration *decl, NVariableDeclaration *var){
	for(VariableList::iterator it = decl->arguments->begin(); it != decl->arguments->end(); it++){
		if((*it)->id->name.compare(var->id->name) == 0)
			return true;
	}

	for(VariableList::iterator it = decl->returns->begin(); it != decl->returns->end(); it++){
		if((*it)->id->name.compare(var->id->name) == 0)
			return true;
	}
	return false;
}

void generate_runtime(NFunctionDeclaration* target, FunctionList *modules){
	//Fix up the arguments so we get enough memory to do this
	//TODO: try to free and reuse memory from a void typed pool by paying attention to when memory goes
	//out of scope. potentially find optimal order of modules to minimize memory consumption
	target->block->children.clear();
	for(FunctionList::iterator it = modules->begin(); it!=modules->end(); it++){
		NFunctionDeclaration *decl = *it;
		for(VariableList::iterator it2 = decl->returns->begin(); it2 != decl->returns->end(); it2++){
			NVariableDeclaration *var = *it2;
			if(!varPresent(target,var)){
				//Add it
				NVariableDeclaration *new_var = (NVariableDeclaration*)var->clone();
				//TODO: HACK!!
				new_var->type->isArray = 1;
				target->AddReturn(new_var);
			}
		}
	}

	//Add the modules to the function body
	for(FunctionList::iterator it = modules->begin(); it!=modules->end(); it++){
		target->block->add_child(new NLoop(0));
	}	
}

////////////////////////OLD PLAN////////////////////////////////////

//TODO: this is real fugly for now. need a better way of defining input and output arrays
RuntimeInst::RuntimeInst(NFunctionDeclaration* func){
	name = func->id->name;

	VariableList::iterator it;
	for(it = func->arguments->begin(); it!=func->arguments->end(); it++){
			inputs.push_back(*it);
	}

	for(it = func->returns->begin(); it!=func->returns->end(); it++){
			outputs.push_back(*it);
	}
}

void RuntimeInst::print(){
	cout << "__global__ void " << name << "_runtime(";
	
	bool first=true;
	VariableList::iterator it;
	for(it = inputs.begin(); it!= inputs.end(); it++){
		if(first){
			first=false;
		}else
			cout << ", ";
		NVariableDeclaration *var = *it;
		cout << var->type->name << " *" << var->id->name;
		cout << ", int32 " << var->id->name << "_size";
	}

	for(it = outputs.begin(); it!= outputs.end(); it++){
		if(first){
			first=false;
		}else
			cout << ", ";
		NVariableDeclaration *var = *it;
		cout << var->type->name << " *" << var->id->name;
		cout << ", int32 *" << var->id->name << "_size";
	}

	cout << "){\n";
///////////// TODO: generate this moar
	cout << "\tfor(int i=0; i<N; i+=blockDim.x*gridDim.x){\n";
  	cout << "\t\tint idx = i + blockIdx.x * blockDim.x + threadIdx.x;\n";
	cout << "\t\tif (idx<N)\n";
	cout << "\t\t\t" << name << "(idx,return,input)\n";
  	cout << "\t}\n";
	cout << "\t*return_size = input_size;\n";
////////////
	cout << "}\n";
}

void Runtime::print(){
	auto it = runtimes.begin();
	for(; it!=runtimes.end(); it++){
		(*it).second->print();
	}
}	
