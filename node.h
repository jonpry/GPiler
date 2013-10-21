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
}; 

class Node {
public:
	virtual ~Node() {}
	virtual llvm::Value* codeGen(CodeGenContext& context) { }

	NodeList children;

	virtual void print(ostream& os) { os << "Unknown Node\n"; }
	friend ostream& operator<<(ostream& os, Node &node)
	{
		for(int i=0; i < sTabs; i++)
			os << "\t";
	    	node.print(os);
    		return os;
	}

	static int sTabs;  
};

class NExpression : public Node {
public: 
	void print(ostream& os) { os << "Unknown Expression\n"; }
	virtual GType GetType(CodeGenContext& context) { cout << "Unknown type\n"; }
};

class NStatement : public Node {
public: 
	void print(ostream& os) { os << "Unknown Statement\n"; }
};

class NInteger : public NExpression {
public:
	long long value;
	NInteger(long long value) : value(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Integer: " << value << "\n"; }
	virtual GType GetType(CodeGenContext& context) { return GType(INT_TYPE,32);}
};

class NDouble : public NExpression {
public:
	double value;
	NDouble(double value) : value(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Double: " << value << "\n"; }
	virtual GType GetType(CodeGenContext& context) { return GType(FLOAT_TYPE,64);}
};

class NIdentifier : public NExpression {
public:
	std::string name;
	NIdentifier(const std::string& name) : name(name) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Identifier: " << name << "\n"; }
	GType GetType(CodeGenContext& context);
};

class NMap : public NExpression {
public:
	NIdentifier* name;
	IdList *vars;
	NExpression* expr;
	string anon_name;

	NMap(NIdentifier* name, IdList *vars, NExpression *expr) : name(name), vars(vars), expr(expr) { 
		children.push_back(name);
		for(IdList::iterator it = vars->begin(); it!=vars->end(); it++)
			children.push_back(*it);
		children.push_back(expr);
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
	GType GetType(CodeGenContext& context);
};

class NMethodCall : public NExpression {
public:
	NIdentifier* id;
	ExpressionList* arguments;
	NMethodCall(NIdentifier *id, ExpressionList* arguments) : id(id), arguments(arguments) { 
		children.push_back(id);
		if(arguments){
			for(ExpressionList::iterator it = arguments->begin(); it != arguments->end(); it++){
				children.push_back(*it);
			}
		}
	}
	NMethodCall(NIdentifier *id) : id(id) {
		children.push_back(id);
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
		children.push_back(dest);
		children.push_back(src);
		if(chain){
			for(MapList::iterator it = chain->begin(); it!=chain->end(); it++){
				children.push_back(*it);
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
		children.push_back(lhs);
		children.push_back(rhs);
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);

	void print(ostream& os) { 
		os << "Binary Op: ";
		switch(op){
			case TPLUS:	os << "+\n"; break;
			case TMINUS: 	os << "-\n"; break;
			case TMUL:	os << "*\n"; break;
			case TDIV:	os << "/\n"; break;
			case TCGT:	os << ">\n"; break;
			default:	os << "unknown op\n"; break; 
		}
		sTabs++;
		os << *lhs;
		os << *rhs;
		sTabs--;
	}
};

class NIf : public NExpression {
public:
	NExpression *pred, *yes, *no;
	NIf(NExpression *pred, NExpression *yes, NExpression *no) : pred(pred), yes(yes), no(no) { 
		children.push_back(pred);
		children.push_back(yes);
		children.push_back(no);
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);

	void print(ostream& os) { 
		os << "If:\n";
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
		children.push_back(array);
		children.push_back(index);
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

	void init(){
		if(lhs)
			children.push_back(lhs);
		children.push_back(rhs);
		if(array)
			children.push_back(array);
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
		os << *id;
		if(assignmentExpr)
			os << *assignmentExpr;
		sTabs--;
	}
};

class NFunctionDeclaration : public NStatement {
public:
	NType *type;
	NIdentifier *id;
	VariableList *arguments;
	NBlock *block;
	NFunctionDeclaration(NType* type, NIdentifier* id, VariableList* arguments, NBlock *block) :
			type(type), id(id), arguments(arguments), block(block) { 
		children.push_back(type);
		children.push_back(id);
		if(arguments){
			for(VariableList::iterator it = arguments->begin(); it!= arguments->end(); it++)
				children.push_back(*it);
		}
		children.push_back(block);
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		os << "Function decl:\n"; 
		sTabs++;
		os << *type << *id;
		VariableList::iterator it;
		for(it = arguments->begin(); it!=arguments->end(); it++)
			os << **it;
		os << *block;	
 		sTabs--;
	}

	void SetType(NType* new_type){
		children.remove(type);
		type = new_type;
	}

	void InsertArgument(VariableList::iterator at, NVariableDeclaration *var){
		arguments->insert(at,var);
		children.push_back(var);
	}
};

#endif
