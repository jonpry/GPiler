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
#include "codegen.h"
#include "runtime.h"

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


void split_unnatural_rcv(NFunctionDeclaration *decl, string base_name, int num_splits, FunctionList *split_funcs){
	ExpressionList naturals,taints,unnaturals;
	AssignmentList assignments;

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

		if(assn && (natural != UNNATURAL)){
			assignments.push_back(assn);
		}
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
	char func_name[128];
	sprintf(func_name,"%s.%d",base_name.c_str(),num_splits);
	decl->id->name = func_name;

	split_funcs->push_back(decl);

	//For every tainted value, create a new function with that value as an argument, add all of the unnatural assignments
	//and try to do some recursion
	for(ExpressionList::iterator it=taints.begin(); it!= taints.end(); it++){
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

		
		NFunctionDeclaration *new_func = new NFunctionDeclaration(0,new NIdentifier("foo"),args,block);

		split_unnatural_rcv(new_func,base_name,num_splits+1,split_funcs);
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


	// Zonk the return type and rebuild it
	decl->ClearReturns();
	{
		for(ExpressionList::iterator it=taints.begin(); it!= taints.end(); it++){
			NVariableDeclaration *input = dynamic_cast<NVariableDeclaration*>(*it);
			assert(input);
			NVariableDeclaration *new_var = (NVariableDeclaration*)input->clone();
			new_var->SetExpr(0);
			decl->AddReturn(new_var);

			//Replace the declaration with an assignment
			NAssignment *assn = new NAssignment(input->id,input->assignmentExpr);
			for(NodeList::iterator it2=decl->block->children.begin(); it2!=decl->block->children.end(); it2++){
				if((*it2)==input){
//					cout << "Found\n";
					it2 = decl->block->children.erase(it2);
					decl->block->children.insert(it2,assn);
					break;
				}
			} 
		}

		for(AssignmentList::iterator it=assignments.begin(); it!= assignments.end(); it++){
			NAssignment* assn = *it;
			//TODO: fubar that we use (NIdentifier*)assn->rhs
			NType* type = typeOf(decl,(NIdentifier*)assn->rhs,1);
			type->isArray=1;
			NVariableDeclaration *ret = new NVariableDeclaration(type,assn->lhs);
			decl->AddReturn(ret);
		}
	}	
}

void split_unnatural(NFunctionDeclaration *decl, FunctionList *split_funcs){
	//TODO: this function assumes that all input arrays can be of different size
	//Maps of multiple inputs and such things may put restrictions on that allowing
	//more code to be combined into modules

	for(VariableList::iterator it=decl->arguments->begin(); it!=decl->arguments->end(); it++){
		NFunctionDeclaration *new_decl = (NFunctionDeclaration*)decl->clone();
		//cout << *new_decl;

		new_decl->ClearArguments();
		new_decl->AddArgument(*it);

		split_unnatural_rcv(new_decl,decl->id->name,split_funcs->size(),split_funcs);
	}
}

//TODO: we need a way to do this stuff that isn't specific to filter, probably through better support of multiple returns
void expand_filter(NFunctionDeclaration *decl){
	//Code assume that filter will be stuck in an NAssignment, which is true at time of writing
	for(NodeList::iterator it = decl->block->children.begin(); it!=decl->block->children.end(); it++){
		NAssignment *assn = dynamic_cast<NAssignment*>(*it);
		if(!assn)
			continue;
		NMap* map = dynamic_cast<NMap*>(assn->rhs);
		if(!map)
			continue;
		if(map->name->name.compare("filter")!=0)
			continue;

		//Found it
		char new_name[128];
		sprintf(new_name, "%s.pred", assn->lhs->name.c_str());
		assn->rhs = map->input;
		NAssignment *new_assn = new NAssignment(new NIdentifier(new_name),map);
		decl->block->children.insert(it,new_assn);

		NVariableDeclaration *new_decl = new NVariableDeclaration(new NType("bool",0), new NIdentifier(new_name));
		decl->AddReturn(new_decl);
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
		//TODO: for now we just copy the thing so the constructed runtime doesn't end up
		//in programBlock until we are ready for it
		decl = (NFunctionDeclaration*)decl->clone();
		NFunctionDeclaration *cpy = (NFunctionDeclaration*)decl->clone();
		FunctionList split_funcs;
		split_unnatural(cpy,&split_funcs);

		for(FunctionList::iterator it2 = split_funcs.begin(); it2!=split_funcs.end(); it2++){
			//Some per function subpasses to do here
			expand_filter(*it2);
			cout << **it2;
		}

		generate_runtime(decl,&split_funcs);
		cout << *decl;
	}
}

