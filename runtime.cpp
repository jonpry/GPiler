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

//TODO: this is real fugly for now. need a better way of defining input and output arrays
RuntimeInst::RuntimeInst(NFunctionDeclaration* func){
	name = func->id->name;

	int count=0;
	VariableList::iterator it;
	for(it = func->arguments->begin(); it!=func->arguments->end(); it++){
		if(count++==0)
			outputs.push_back(*it);
		else
			inputs.push_back(*it);
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
