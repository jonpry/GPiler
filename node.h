/* 
GPiler - node.h
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

#ifndef NODE_H
#define NODE_H

#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <typeinfo>
#include <llvm/IR/Value.h>

class Node;
class NBlock;
class NExpression;
class NStatement;
class NIdentifier;
class NType;
class NVariableDeclaration;
class NPipeLine;
class NMap;

#include "parser.hpp"

#define MAX(a,b) (a>b?a:b)

using namespace std;

class CodeGenContext;
class NStatement;
class NExpression;
class NVariableDeclaration;

typedef list<NStatement*> StatementList;
typedef list<NExpression*> ExpressionList;
typedef list<NVariableDeclaration*> VariableList;
typedef list<NIdentifier*> IdList;
typedef list<NMap*> MapList;
typedef list<Node*> NodeList;

#define INT_TYPE 1
#define FLOAT_TYPE 2
#define BOOL_TYPE 3
#define VOID_TYPE 4

struct GType {
	int type;
	int length;
	GType(int type, int length) : type(type), length(length) {}
	GType(){}

	void print();
	NType* toNode();
}; 

GType promoteType(GType ltype, GType rtype);
int isCmp(int op);

class Node {
public:
	virtual ~Node() {}
	virtual llvm::Value* codeGen(CodeGenContext& context) { }

	NodeList children;
	Node *parent;

	virtual void print(ostream& os) { os << "Unknown Node: " << typeid(*this).name() << "\n"; }
	friend ostream& operator<<(ostream& os, Node &node)
	{
		for(int i=0; i < sTabs; i++)
			os << "\t";
	    	node.print(os);
    		return os;
	}

	void add_child(Node *c){
		children.push_back(c);
		c->parent = this;
	}

	void add_child(NodeList::iterator at, Node *c){
		children.insert(at,c);
		c->parent = this;
	}

	static int sTabs;  
};

class NExpression : public Node {
public: 
	void print(ostream& os) { os << "Unknown Expression: " <<  typeid(*this).name() << "\n"; }
	virtual GType GetType(map<std::string, GType> &locals) { cout << "Unknown type: " <<  typeid(*this).name() << "\n"; }
	virtual GType GetType(map<std::string, GType> &locals, GType* ptype, int* found, NExpression* exp) { cout << "Unknown type2: " <<  typeid(*this).name() << "\n"; }
};

class NStatement : public Node {
public: 
	void print(ostream& os) { os << "Unknown Statement: " <<  typeid(*this).name() << "\n"; }
};

class NInteger : public NExpression {
public:
	long long value;
	NInteger(long long value) : value(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Integer: " << value << "\n"; }
	GType GetType(map<std::string, GType> &locals) { return GType(INT_TYPE,32);}
};

class NDouble : public NExpression {
public:
	double value;
	NDouble(double value) : value(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Double: " << value << "\n"; }
	GType GetType(map<std::string, GType> &locals) { return GType(FLOAT_TYPE,64);}
};

class NIdentifier : public NExpression {
public:
	std::string name;
	NIdentifier(const std::string& name) : name(name) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Identifier: " << name << "\n"; }
	GType GetType(map<std::string, GType> &locals);
};

class NMap : public NExpression {
public:
	NIdentifier* name;
	IdList *vars;
	NExpression* expr;
	string anon_name;

	NMap(NIdentifier* name, IdList *vars, NExpression *expr) : name(name), vars(vars), expr(expr) { 
		add_child(name);
		for(IdList::iterator it = vars->begin(); it!=vars->end(); it++)
			add_child(*it);
		add_child(expr);
	}

//	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		os << "Map:\n";
		sTabs++;
		os << *name;
		IdList::iterator it;
		for(it = vars->begin(); it != vars->end(); it++){
			os << ** it;
		} 
		os << *expr;
		sTabs--;
	}
};

class NType: public NIdentifier {
public:
	int isArray;
	NType(const std::string& name, int isArray) : NIdentifier(name), isArray(isArray) { }	
//	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Type: " << name << " " << isArray << "\n"; }
	GType GetType(map<std::string, GType> &locals);
};

class NMethodCall : public NExpression {
public:
	NIdentifier* id;
	ExpressionList* arguments;
	NMethodCall(NIdentifier *id, ExpressionList* arguments) : id(id), arguments(arguments) { 
		add_child(id);
		if(arguments){
			for(ExpressionList::iterator it = arguments->begin(); it != arguments->end(); it++){
				add_child(*it);
			}
		}
	}
	NMethodCall(NIdentifier *id) : id(id) {
		add_child(id);
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);

	void print(ostream& os) { 
		os << "MethodCall:\n";
		sTabs++;
		os << *id;
		ExpressionList::iterator it;
		for(it = arguments->begin(); it!=arguments->end(); it++)
			os << ** it;
		sTabs--;
	}
};

class NPipeLine : public NExpression {
public:
	NIdentifier *dest, *src;
	MapList *chain;
	NPipeLine(NIdentifier *src, NIdentifier *dest, MapList *chain) : src(src), chain(chain), dest(dest) {
		add_child(dest);
		add_child(src);
		if(chain){
			for(MapList::iterator it = chain->begin(); it!=chain->end(); it++){
				add_child(*it);
			}
		}
 	}
//	virtual llvm::Value* codeGen(CodeGenContext& context);

	void print(ostream& os) { 
		os << "PipeLine:\n";
		sTabs++;
		os << *src;
		os << *dest;
		MapList::iterator it;
		for(it = chain->begin(); it!=chain->end(); it++){
			os << **it;
		}
		sTabs--;
	}
};


class NBinaryOperator : public NExpression {
public:
	int op;
	NExpression *lhs, *rhs;
	NBinaryOperator(NExpression *lhs, int op, NExpression *rhs) : lhs(lhs), rhs(rhs), op(op) { 
		add_child(lhs);
		add_child(rhs);
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);

	GType GetType(map<std::string, GType> &locals, GType* ptype, int *found, NExpression* exp) { 
		if(exp==this){
			*ptype = GetType(locals);
			*found=1;
			return *ptype;
		}

		GType ret = rhs->GetType(locals,ptype,found,exp);
		if(*found)
			return ret;

		ret = lhs->GetType(locals,ptype,found,exp);
		if(*found)
			return ret;

		return GetType(locals);	
	}

	void print(ostream& os) { 
		os << "Binary Op: ";
		switch(op){
			case TPLUS:	os << "+\n"; break;
			case TMINUS: 	os << "-\n"; break;
			case TMUL:	os << "*\n"; break;
			case TDIV:	os << "/\n"; break;
			case TCGT:	os << ">\n"; break;
			case TCLT:	os << "<\n"; break;
			case TCGE:	os << ">=\n"; break;
			case TCLE:	os << "<=\n"; break;
			case TCNE:	os << "!=\n"; break;	
			case TCEQ:	os << "==\n"; break;
			default:	os << "unknown op: " << op << "\n"; break; 
		}
		sTabs++;
		os << *lhs;
		os << *rhs;
		sTabs--;
	}

	GType GetType(map<std::string, GType> &locals);
};

class NSelect : public NExpression {
public:
	NExpression *pred, *yes, *no;
	NSelect(NExpression *pred, NExpression *yes, NExpression *no) : pred(pred), yes(yes), no(no) { 
		add_child(pred);
		add_child(yes);
		add_child(no);
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);


	GType GetType(map<std::string, GType> &locals, GType* ptype, int *found, NExpression* exp) { 
		if(exp==this){
			*ptype = GetType(locals);
			*found=1;
			return *ptype;
		}

		GType ret = yes->GetType(locals,ptype,found,exp);
		if(*found)
			return ret;

		ret = no->GetType(locals,ptype,found,exp);
		if(*found)
			return ret;

		ret = pred->GetType(locals,ptype,found,exp);
		if(*found)
			return ret;

		return GetType(locals);	
	}

	GType GetType(map<std::string, GType> &locals);

	void print(ostream& os) { 
		os << "Select:\n";
		sTabs++;
		os << *pred;
		os << *yes;
		os << *no;
		sTabs--;
	}
};

class NArrayRef : public NExpression {
public:
	NIdentifier *array, *index;
	NArrayRef(NIdentifier *array, NIdentifier *index) : array(array), index(index) { 
		add_child(array);
		add_child(index);
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);
	llvm::Value* store(CodeGenContext& context, llvm::Value* rhs);

	void print(ostream& os) { 
		os << "Array Ref:\n";
		sTabs++;
		os << *array;
		os << *index;
		sTabs--;
	}
};

class NAssignment : public NExpression {
public:
	NIdentifier *lhs;
	NExpression *rhs;
	NArrayRef *array;
	int isReturn;
	NAssignment(NIdentifier *lhs, NExpression *rhs) : array(0), lhs(lhs), rhs(rhs), isReturn(0) {init(); }
	NAssignment(NArrayRef *array, NExpression *rhs) : array(array), lhs(0), rhs(rhs), isReturn(0) {init(); }
	virtual llvm::Value* codeGen(CodeGenContext& context);

	GType GetType(map<std::string, GType> &locals, GType* ptype, int *found, NExpression* exp) { 
		if(exp==this){
			*ptype = rhs->GetType(locals);
			*found=1;
			return *ptype;
		}

		GType ret = rhs->GetType(locals,ptype,found,exp);
		if(*found)
			return ret;

		locals[lhs->name] = ret;
		return ret;	
	}

	void init(){
		if(lhs)
			add_child(lhs);
		add_child(rhs);
		if(array)
			add_child(array);
	}
	void print(ostream& os) 
	{ 
		os << "Assignment: return: " << isReturn << "\n";
		sTabs++;
		if(lhs)
			os << *lhs;
		else
			os << *array;
		os << *rhs; 
		sTabs--;
	}
};

class NBlock : public NExpression {
public:
	NBlock() { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		os << "Block: \n"; 
		sTabs++;
		for(NodeList::iterator it = children.begin(); it!= children.end(); it++){
			os << **it;
		}
		sTabs--;
	}
	GType GetType(map<std::string, GType> &locals, GType* ptype, int *found, NExpression* exp) { 
		NodeList::iterator it;
		for(it = children.begin(); it!= children.end(); it++){
			GType ret = ((NExpression*)(*it))->GetType(locals,ptype,found,exp);
			if(*found)
				return ret;
		}
	}
};

class NVariableDeclaration : public NExpression {
public:
 	NType *type;
	NIdentifier *id;
	NExpression *assignmentExpr;
	NVariableDeclaration(NType *type, NIdentifier *id) : type(type), id(id) { }
	NVariableDeclaration(NType *type, NIdentifier *id, NExpression *assignmentExpr) : type(type), id(id), assignmentExpr(assignmentExpr) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		os << "Variable decl:\n"; 
		sTabs++;
		os << *type;
		if(id)
			os << *id;
		if(assignmentExpr)
			os << *assignmentExpr;
		sTabs--;
	}

	void SetType(NType* new_type){
		children.remove(type);
		add_child(new_type);
		type = new_type;
	}

	GType GetType(map<std::string, GType> &locals){ return type->GetType(locals); }
};

class NFunctionDeclaration : public NStatement {
public:
	NIdentifier *id;
	VariableList *returns, *arguments;
	NBlock *block;
	int isGenerated;
	NFunctionDeclaration(VariableList* returns, NIdentifier* id, VariableList* arguments, NBlock *block) :
			returns(returns), id(id), arguments(arguments), block(block), isGenerated(0) { 
		add_child(id);
		if(returns){
			for(VariableList::iterator it = returns->begin(); it!= returns->end(); it++)
				add_child(*it);
		}
		if(arguments){
			for(VariableList::iterator it = arguments->begin(); it!= arguments->end(); it++)
				add_child(*it);
		}
		add_child(block);
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		os << "Function decl:\n"; 
		sTabs++;
		os << *id;
		VariableList::iterator it;
		for(it = returns->begin(); it!=returns->end(); it++)
			os << **it;
		for(it = arguments->begin(); it!=arguments->end(); it++)
			os << **it;
		os << *block;	
 		sTabs--;
	}

	void SetType(VariableList &new_types){
		VariableList::iterator it;
		for(VariableList::iterator it = returns->begin(); it!= returns->end(); it++)
			children.remove(*it);
		returns->clear();
		for(VariableList::iterator it = new_types.begin(); it!= new_types.end(); it++){
			returns->push_back(*it);
			add_child(*it);
		}
	}

	void InsertArgument(VariableList::iterator at, NVariableDeclaration *var){
		arguments->insert(at,var);
		add_child(var);
	}

	int isScalar(){
		for(VariableList::iterator it = returns->begin(); it!= returns->end(); it++)
			if((*it)->type->isArray)
				return 0;
		for(VariableList::iterator it = arguments->begin(); it!= arguments->end(); it++)
			if((*it)->type->isArray)
				return 0;
		return 1;
	}
};

#endif
