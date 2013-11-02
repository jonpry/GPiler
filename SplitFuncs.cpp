/* 
GPiler - SlitFuncs.cpp
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

#define NATURAL 	0
#define TAINTED 	1
#define UNNATURAL 	2

int isNatural(NExpression *expr, NFunctionDeclaration *decl);

int isNatural(NIdentifier *id, NFunctionDeclaration *decl) {
	for(VariableList::iterator it = decl->arguments->begin(); it != decl->arguments->end(); it++){
		NVariableDeclaration *vdec = *it;
		if(vdec->id->name.compare(id->name) == 0){
			return NATURAL;
		}
	}

	for(NodeList::iterator it = decl->block->children.begin(); it != decl->block->children.end(); it++){
		NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it);
		if(vdec && vdec->id->name.compare(id->name) == 0){
			return isNatural(vdec->assignmentExpr,decl)?UNNATURAL:NATURAL;	
		}
	}
	
	return UNNATURAL;
}

int isNatural(NExpression* expr, NFunctionDeclaration *decl){
	NMap* map = dynamic_cast<NMap*>(expr);
	if(map){
		if(!map->isNatural())
			return isNatural(map->input,decl)?UNNATURAL:TAINTED;
		return isNatural(map->input,decl)?UNNATURAL:NATURAL;
	}

	NIdentifier *id = dynamic_cast<NIdentifier*>(expr);
	if(id){
		return isNatural(id,decl)?UNNATURAL:NATURAL;
	}

	cout << "IsNatural failed2\n";
	cout << *expr;
	exit(-1);
	return -1;
}


void split_unnatural(NFunctionDeclaration *decl, FunctionList *split_funcs){
	ExpressionList naturals,taints,unnaturals;

	for(NodeList::iterator it = decl->block->children.begin(); it != decl->block->children.end(); it++){
		int natural=NATURAL;
		NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it);
		if(vdec){
			natural = isNatural(vdec->assignmentExpr,decl);
		}
		NAssignment *assn = dynamic_cast<NAssignment*>(*it);
		if(assn){
			natural = isNatural(assn->rhs,decl);
		}
		assert(vdec || assn);
		//Store tainted values, ignore UNNATURAL
		if(natural==NATURAL)
			naturals.push_back((NExpression*)*it);
		else if(natural==TAINTED)
			taints.push_back((NExpression*)*it);
		else
			unnaturals.push_back((NExpression*)*it);
	}

#if 0
	cout << "Naturals\n";

	for(ExpressionList::iterator it=naturals.begin(); it!= naturals.end(); it++){
		cout << **it << ";\n";
	}

	cout << "Taints\n";

	for(ExpressionList::iterator it=taints.begin(); it!= taints.end(); it++){
		cout << **it << ";\n";
	}
#endif
	//Add the current function to the list of splits
	split_funcs->push_back(decl);

	//For every tainted value, create a new function with that value as an argument, add all of the unnatural assignments
	//and try to do some recursion
	for(ExpressionList::iterator it=taints.begin(); it!= taints.end(); it++){
		string func_name = "foo";
		VariableList *args = new VariableList();
		NVariableDeclaration *input = dynamic_cast<NVariableDeclaration*>(*it);
		assert(input);
		input = (NVariableDeclaration*)input->clone();
		input->SetExpr(0);
	
		args->push_back(input);
		NBlock *block = new NBlock();
		
		for(ExpressionList::iterator it2 = unnaturals.begin(); it2 != unnaturals.end(); it2++){
			block->add_child((*it2)->clone());
		}

		
		NFunctionDeclaration *new_func = new NFunctionDeclaration(0,new NIdentifier(func_name),args,block);

		split_unnatural(new_func,split_funcs);
	}

	//remove everything but taints and naturals from the current function
	{
		decl->block->children.clear();
		for(ExpressionList::iterator it=naturals.begin(); it!= naturals.end(); it++){
			decl->block->add_child(*it);
		}

		for(ExpressionList::iterator it=taints.begin(); it!= taints.end(); it++){
			decl->block->add_child(*it);
		}
	}
}

//this function requires conversion to strict SSA first, only elements are assignment for return and vardec's
void split_unnatural(NBlock *pb){
	NodeList::iterator it;
	NodeList kernels;
	for(it = pb->children.begin(); it != pb->children.end(); it++){
		NFunctionDeclaration *decl = dynamic_cast<NFunctionDeclaration*> (*it);
		if(decl && !decl->isGenerated){
			kernels.push_back(decl);
		}
	}

	for(it = kernels.begin(); it != kernels.end(); it++){
		NFunctionDeclaration *decl =(NFunctionDeclaration*) (*it);
		NFunctionDeclaration *cpy = (NFunctionDeclaration*)decl->clone();
		FunctionList split_funcs;
		split_unnatural(cpy,&split_funcs);

		for(FunctionList::iterator it2 = split_funcs.begin(); it2!=split_funcs.end(); it2++){
			cout << **it2;
		}
	}
}

