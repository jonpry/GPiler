/* 
GPiler - types.cpp
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

using namespace std;


//Brute force the program block to determine the implied type of an expression anywhere in the program
GTypeList GetType(NBlock *pb, Node *exp){
	map<std::string, GTypeList> locals;

	Node *parent = exp->parent;
	while(1){
		NFunctionDeclaration *funcp = dynamic_cast<NFunctionDeclaration*>(parent);
		if(funcp)
			break;
		parent = parent->parent;
	}
	NFunctionDeclaration *func = (NFunctionDeclaration *)parent;

	//Find return type for all functions
	{
		NodeList::iterator it;
		for(it=pb->children.begin(); it!= pb->children.end(); it++){
			if(*it != parent){
				NFunctionDeclaration *func = dynamic_cast<NFunctionDeclaration*>(parent);
				if(!func)
					continue;
				locals[func->id->name] = func->GetType(locals); 
			}
		}
	}

	//Add the functions arguments
	{
		VariableList::iterator it;
		for(it = func->arguments->begin(); it != func->arguments->end(); it++){
			locals[(*it)->id->name] = (*it)->GetType(locals);
		}
	}

	//TODO: this function may not work right because it doesn't push context on basic blocks.

	GTypeList exptype;
	int found=0;
	func->block->GetType(locals,&exptype,&found,exp);

	return exptype;			
}

void GType::print(){
	cout << type << ", " << length << "\n";
}

TypeList ntypesOf(GTypeList in, int allowArray){
	TypeList ret;
	for(GTypeList::iterator it = in.begin(); it != in.end(); it++){
		NType* type = (*it).toNode();
		if(!allowArray)
			type->isArray=0;
		ret.push_back(type);
	}
	return ret;
}

TypeList typeOf(NFunctionDeclaration *decl, IdList vars, int allowArray){
	TypeList ret;
	for(IdList::iterator it=vars.begin(); it!=vars.end(); it++){
		ret.splice(ret.end(),typeOf(decl,*it,allowArray));
	}
	return ret;
}

TypeList typeOf(NFunctionDeclaration *decl, NIdentifier *var, int allowArray){
	for(VariableList::iterator it = decl->arguments->begin(); it != decl->arguments->end(); it++){
		NVariableDeclaration *vdec = *it;
		if(vdec->id->name.compare(var->name) == 0){
			return ntypesOf(((Node*)vdec)->GetType(), allowArray);
		}
	}
	if(decl->returns){
		for(VariableList::iterator it = decl->returns->begin(); it != decl->returns->end(); it++){	
			NVariableDeclaration *vdec = *it;
			if(vdec->id->name.compare(var->name) == 0){
				return ntypesOf(((Node*)vdec)->GetType(), allowArray);
			}
		}
	}

	for(NodeList::iterator it = decl->block->children.begin(); it != decl->block->children.end(); it++){
		NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it);
		if(vdec && vdec->id->name.compare(var->name) == 0){
			return ntypesOf(((Node*)vdec)->GetType(), allowArray);
		}
	}
	cout << "Couldn't find var: " << var->name << "\n";
	exit(-1);
	return TypeList();
}

TypeList typeOf(string name, NBlock* pb){
	NodeList::iterator it;
	for(it = pb->children.begin(); it!=pb->children.end(); it++){
		NFunctionDeclaration *func = dynamic_cast<NFunctionDeclaration*> (*it);
		if(func) {
			if(name.compare(func->id->name)==0){
				//Return all function types
				map<std::string, GTypeList> locals;
				return ntypesOf(func->GetType(locals),0);
			}
		}
	}
	cout << "Function not found: " << name << "\n";
	exit(-1);
	return TypeList();
}

NType* GType::toNode(){
	string stype;
	if(type==FLOAT_TYPE){
		if(length==64)
			stype = "double";
		else
			stype = "float";
	}

	if(type==INT_TYPE){
		switch(length){
			case 64: stype = "int64"; break;
			case 32: stype = "int32"; break;
			case 16: stype = "int16"; break;
			case 8: stype = "int8"; break;

		}
	}

	if(type==VOID_TYPE){
		stype = "void";
	}

	if(type==BOOL_TYPE){
		stype = "bool";
	}

	NType* ret = new NType(stype,isArray);
	ret->isPointer = isPointer;
	return ret;
}

GTypeList NBinaryOperator::GetType(map<std::string, GTypeList> &locals){
	if(isCmp(op)){
		return GTypeList{GType(BOOL_TYPE,1,0)};
	}
	return promoteType(lhs->GetType(locals),rhs->GetType(locals));
}

GTypeList NSelect::GetType(map<std::string, GTypeList> &locals){
	return promoteType(yes->GetType(locals),no->GetType(locals));
}

GTypeList promoteType(GTypeList ltypel, GTypeList rtypel){
	GType ltype = *ltypel.begin();
	GType rtype = *rtypel.begin();
	if(ltypel.size() > 1 || rtypel.size() > 1){
		cout << "Arithmetic on vectors found\n";
		exit(-1);
	}
	GType ret;
	if(ltype.type == FLOAT_TYPE || rtype.type == FLOAT_TYPE){
		ret.type = FLOAT_TYPE;
	}else{
		//TODO: not sure if we support any other automatic promotions but we can at least check
		//for argument type match and throw awesome error
		ret.type = ltype.type;
	}

	ret.length = MAX(ltype.length,rtype.length);

	return GTypeList{ret};
}

GTypeList NIdentifier::GetType(map<std::string, GTypeList> &locals) { 
	return locals[name];
}

GTypeList NType::GetType(map<std::string, GTypeList> &locals){
	GType ret;
	if (name.compare("int") == 0 || name.compare("int32") == 0) {
		ret.type = INT_TYPE; ret.length = 32;
	}else if (name.compare("int64") == 0) {
		ret.type = INT_TYPE; ret.length = 64;
	}else if (name.compare("double") == 0) {
		ret.type = FLOAT_TYPE; ret.length = 64;
	}else if (name.compare("float") == 0) {
		ret.type = FLOAT_TYPE; ret.length = 32;
	}else if (name.compare("int16") == 0) {
		ret.type = INT_TYPE; ret.length = 32;
	}else if (name.compare("int8") == 0) {
		ret.type = INT_TYPE; ret.length = 8;
	}else if (name.compare("bool") == 0) {
		ret.type = BOOL_TYPE; ret.length = 1;
	}else if (name.compare("void") == 0) {
		ret.type = VOID_TYPE; ret.length = 0;
	} else cout << "Error unknown type\n";
	ret.isArray = isArray;
	ret.isPointer = isPointer;
	return GTypeList{ret};
}
