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
	
	cout << "IsNatural failed\n";
	exit(-1);
	return -1;
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


void split_unnatural(NFunctionDeclaration *decl){
	ExpressionList naturals,unnaturals;

	for(NodeList::iterator it2 = decl->block->children.begin(); it2 != decl->block->children.end(); it2++){
		int natural=NATURAL;
		NVariableDeclaration *vdec = dynamic_cast<NVariableDeclaration*>(*it2);
		if(vdec){
			natural = isNatural(vdec->assignmentExpr,decl);
		}
		NAssignment *assn = dynamic_cast<NAssignment*>(*it2);
		if(assn){
			natural = isNatural(assn->rhs,decl);
		}
		//Put TAINTED into NATURAL's list
		if(natural==UNNATURAL)
			unnaturals.push_back((NExpression*)*it2);
		else
			naturals.push_back((NExpression*)*it2);
	}

	cout << "Naturals\n";

	for(ExpressionList::iterator it3=naturals.begin(); it3!= naturals.end(); it3++){
		cout << **it3;
	}

	cout << "Unnaturals\n";

	for(ExpressionList::iterator it3=unnaturals.begin(); it3!= unnaturals.end(); it3++){
		cout << **it3;
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
		split_unnatural(decl);
	}
}
