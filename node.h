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
class NFunctionDeclaration;

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
typedef list<NFunctionDeclaration*> FunctionList;

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
	Node(){}
	//Copy constructor
	Node(const Node& other){
		//None of Node members are valid in deep copy, so do nothing
	}	
	virtual ~Node() {}
	virtual llvm::Value* codeGen(CodeGenContext& context) { return nullptr; }

	NodeList children;
	Node *parent;

	virtual void print(ostream& os) { os << "Unknown Node: " << typeid(*this).name() << "\n"; }
	friend ostream& operator<<(ostream& os, Node &node)
	{
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

	virtual Node* clone() { cout << "Unknown clone: " << typeid(*this).name() << "\n"; return 0; }

	static int sTabs;  
};

class NExpression : public Node {
public: 
	void print(ostream& os) { os << "Unknown Expression: " <<  typeid(*this).name() << "\n"; }
	virtual GType GetType(map<std::string, GType> &locals) { cout << "Unknown type: " <<  typeid(*this).name() << "\n"; return GType(); }
	virtual GType GetType(map<std::string, GType> &locals, GType* ptype, int* found, NExpression* exp) { cout << "Unknown type2: " <<  typeid(*this).name() << "\n"; return GType(); }
	virtual void GetIdRefs(IdList &list) { cout << "Unknown idref: " <<  typeid(*this).name() << "\n"; }
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
	void print(ostream& os) { os << value; }
	GType GetType(map<std::string, GType> &locals) { return GType(INT_TYPE,32);}

	Node* clone() { return new NInteger(*this); }
};

class NDouble : public NExpression {
public:
	double value;
	NDouble(double value) : value(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << value; }
	GType GetType(map<std::string, GType> &locals) { return GType(FLOAT_TYPE,64);}

	Node* clone() { return new NDouble(*this); }
};

// name for a variable or function which is unique in its scope.
class NIdentifier : public NExpression {
public:
	std::string name;
	NIdentifier(const std::string& name) : name(name) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << name; }
	GType GetType(map<std::string, GType> &locals);
	void GetIdRefs(IdList &list) { list.push_back(this); }

	Node* clone(){
		return new NIdentifier(*this);
	}
};

class NMap : public NExpression {
public:
	NIdentifier* name, *input;
	IdList *vars;
	NExpression* expr;
	string anon_name;

	NMap(NIdentifier* name, IdList *vars, NExpression *expr) : name(name), input(0), vars(vars), expr(expr) { 
		add_all_children();
	}

	void add_all_children(){
		if(name)
			add_child(name);
		if(input)
			add_child(input);
		for(IdList::iterator it = vars->begin(); it!=vars->end(); it++)
			add_child(*it);
		add_child(expr);
	}

	//Copy constructor
	NMap(const NMap &other){
		name = 0; input = 0; vars = 0; expr = 0;
		anon_name = other.anon_name;
		if(other.name)
			name = (NIdentifier*)other.name->clone();
		if(other.input)
			input = (NIdentifier*)other.input->clone();
		if(other.expr)
			expr = (NExpression*)other.expr->clone();
		if(other.vars){
			vars = new IdList();
			for(IdList::iterator it = other.vars->begin(); it!= other.vars->end(); it++){
				vars->push_back( (NIdentifier*) (*it)->clone() );
			}
		}

		add_all_children();
	}

//	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
//		os << "Map:\n";
//		sTabs++;
		os << *name << "( ";
		IdList::iterator it;
		for(it = vars->begin(); it != vars->end(); it++){
			os << **it << ", ";
		} 
		os << ": " << *expr << ")";
		if(input)
			os << " < " << *input;
//		sTabs--;
	}

	void SetInput(NIdentifier *in){
		children.remove(input);
		add_child(in);
		input = in;
	}

	int isNatural(){
		return !name->name.compare("map");
	}

	void GetIdRefs(IdList &list) { input->GetIdRefs(list); }

	Node* clone() { return new NMap(*this); }
};

class NType: public NIdentifier {
public:
	int isArray;
	NType(const std::string& name, int isArray) : NIdentifier(name), isArray(isArray) { }	
//	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		if(isArray)
			os << "[";
		os << name; 
		if(isArray)
			os << "]";
	}
	GType GetType(map<std::string, GType> &locals);

	Node* clone(){ return new NType(*this); }
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

	void GetIdRefs(IdList &list) { 
		for(ExpressionList::iterator it = arguments->begin(); it!= arguments->end(); it++){
			(*it)->GetIdRefs(list);
		}
	}

	void print(ostream& os) { 
//		os << "MethodCall:\n";
//		sTabs++;
		os << *id << "(";
		ExpressionList::iterator it;
		for(it = arguments->begin(); it!=arguments->end(); it++)
			os << ** it << ", ";
		os << ")";
//		sTabs--;
	}
};

class NPipeLine : public NExpression {
public:
	NIdentifier *dest, *src;
	MapList *chain;
	NPipeLine(NIdentifier *src, NIdentifier *dest, MapList *chain) : dest(dest), src(src), chain(chain) {
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
		os << *src;
		MapList::iterator it;
		for(it = chain->begin(); it!=chain->end(); it++){
			os << " :: " << **it;
		}
		os << " > " << *dest;
	}
};

class NTriad : public NExpression {
public:
	NIdentifier *src,*dst;
	NMap *map;
	NTriad(NIdentifier *src, NMap *map, NIdentifier *dst) : src(src), dst(dst), map(map) {
		add_child(src);
		add_child(dst);
		add_child(map);
	}

	void print(ostream& os) { 
		os << "Triad:\n";
		sTabs++;
		os << *src;
		os << *dst;
		os << *map;
		sTabs--;
	}
};

class NBinaryOperator : public NExpression {
public:
	int op;
	NExpression *lhs, *rhs;
	NBinaryOperator(NExpression *lhs, int op, NExpression *rhs) : op(op), lhs(lhs), rhs(rhs) { 
		add_all_children();
	}
	//Copy constructor
	NBinaryOperator(const NBinaryOperator &other){
		op = other.op;
		lhs = (NExpression*)other.lhs->clone();
		rhs = (NExpression*)other.rhs->clone();
		add_all_children();
	}

	void add_all_children(){
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
//		os << "Binary Op: ";
		os << "(" << *lhs << " ";

		switch(op){
			case TPLUS:	os << "+"; break;
			case TMINUS: 	os << "-"; break;
			case TMUL:	os << "*"; break;
			case TDIV:	os << "/"; break;
			case TCGT:	os << ">"; break;
			case TCLT:	os << "<"; break;
			case TCGE:	os << ">="; break;
			case TCLE:	os << "<="; break;
			case TCNE:	os << "!="; break;	
			case TCEQ:	os << "=="; break;
			default:	os << "unknown op: " << op; break; 
		}
//		sTabs++;
		os << " " << *rhs << ")";
//		sTabs--;
	}

	GType GetType(map<std::string, GType> &locals);

	Node* clone(){ return new NBinaryOperator(*this); }
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
//		os << "Array Ref:\n";
//		sTabs++;
		os << *array << "[" << *index << "]";
//		os << *index;
//		sTabs--;
	}
};

class NAssignment : public NExpression {
public:
	NIdentifier *lhs;
	NExpression *rhs;
	NArrayRef *array;
	int isReturn;
	NAssignment(NIdentifier *lhs, NExpression *rhs) : lhs(lhs), rhs(rhs), array(0), isReturn(0) {add_all_children(); }
	NAssignment(NArrayRef *array, NExpression *rhs) : lhs(0), rhs(rhs), array(array), isReturn(0) {add_all_children(); }
	//Copy constructor
	NAssignment(const NAssignment &other){
		isReturn = other.isReturn;
		lhs = 0; rhs = 0; array = 0;
		if(other.lhs)
			lhs = (NIdentifier*)other.lhs->clone();
		if(other.rhs)
			rhs = (NExpression*)other.rhs->clone();
		if(other.array)
			array = (NArrayRef*)other.array->clone();
	}

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

	void GetIdRefs(IdList &list) { rhs->GetIdRefs(list); }

	void add_all_children(){
		if(lhs)
			add_child(lhs);
		add_child(rhs);
		if(array)
			add_child(array);
	}
	void print(ostream& os) 
	{ 
//		os << "Assignment: return: " << isReturn << "\n";
//		sTabs++;
		if(lhs)
			os << *lhs;
		else
			os << *array;
		os << " = ";
		os << *rhs; 
//		sTabs--;
	}

	void SetArray(NArrayRef *ary){
		if(array)
			children.remove(array);
		add_child(ary);
		if(lhs)
			children.remove(lhs);
		lhs = 0;
		array = ary;
	}

	Node* clone() { return new NAssignment(*this); }
};

class NBlock : public NExpression {
public:
	NBlock() { }
	//Copy constructor
	NBlock(const NBlock &other){
		for(NodeList::const_iterator it=other.children.begin(); it!=other.children.end(); it++){
			add_child( (*it)->clone() );
		}
	}

	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
//		os << "Block: \n"; 
//		sTabs++;

		for(NodeList::iterator it = children.begin(); it!= children.end(); it++){
			for(int i=0; i < sTabs; i++)
				os << "\t";
			os << **it << ";\n";
		}
//		sTabs--;
	}
	GType GetType(map<std::string, GType> &locals, GType* ptype, int *found, NExpression* exp) { 
		NodeList::iterator it;
		for(it = children.begin(); it!= children.end(); it++){
			GType ret = ((NExpression*)(*it))->GetType(locals,ptype,found,exp);
			if(*found)
				return ret;
		}
		cout << "Syntax error, could not find type of block:\n" << this << endl;
		exit(-1);
	}

	Node* clone(){
		return new NBlock(*this);
	}
};

class NVariableDeclaration : public NExpression {
public:
 	NType *type;
	NIdentifier *id;
	NExpression *assignmentExpr;
	NVariableDeclaration(NType *type, NIdentifier *id) : type(type), id(id) { add_all_children(); }
	NVariableDeclaration(NType *type, NIdentifier *id, NExpression *assignmentExpr) : type(type), id(id), assignmentExpr(assignmentExpr) {
		add_all_children();
	}
	//Copy constructor
	NVariableDeclaration(const NVariableDeclaration &other){
		id=0;
		assignmentExpr=0;
		type = (NType*)other.type->clone();
		//cout << *type << ", " << *other.type << "\n";
		if(other.id)
			id = (NIdentifier*)other.id->clone();
		if(other.assignmentExpr)
			assignmentExpr = (NExpression*)other.assignmentExpr->clone();
		add_all_children();
	}

	void add_all_children(){
		add_child(type);
		if(id)
			add_child(id);
		if(assignmentExpr)
			add_child(assignmentExpr);
	}

	llvm::Value* codeGen(CodeGenContext& context);
	void GetIdRefs(IdList &list) { assignmentExpr->GetIdRefs(list); }
	void print(ostream& os) { 
//		os << "Variable decl:\n"; 
//		sTabs++;
		os << *type << " ";
		if(id)
			os << *id;
		if(assignmentExpr)
			os << " = " << *assignmentExpr;
//		sTabs--;
	}

	void SetType(NType* new_type){
		children.remove(type);
		add_child(new_type);
		type = new_type;
	}

	void SetExpr(NExpression* expr){
		children.remove(assignmentExpr);
		if(expr)
			add_child(expr);
		assignmentExpr = expr;
	}

	GType GetType(map<std::string, GType> &locals){ return type->GetType(locals); }

	Node* clone() { return new NVariableDeclaration(*this); }
};

class NFunctionDeclaration : public NStatement {
public:
	NIdentifier *id;
	VariableList *returns, *arguments;
	NBlock *block;
	int isGenerated;
	NFunctionDeclaration(VariableList* returns, NIdentifier* id, VariableList* arguments, NBlock *block) :
			id(id), returns(returns), arguments(arguments), block(block), isGenerated(0) { 
		add_all_children();
	}

	void add_all_children(){
		add_child(id);
		if(returns){
			for(VariableList::iterator it = returns->begin(); it!= returns->end(); it++)
				add_child(*it);
		}
		if(arguments){
			for(VariableList::iterator it = arguments->begin(); it!= arguments->end(); it++)
				add_child(*it);
		}
		if(block)
			add_child(block);
	}

	//Copy constructor
	NFunctionDeclaration(const NFunctionDeclaration& other){
		returns = 0;
		arguments = 0;
		block = 0;
		isGenerated = other.isGenerated;
		id = (NIdentifier*)other.id->clone();
		if(other.block)
			block = (NBlock*)other.block->clone();
		if(other.returns){
			returns = new VariableList();
			for(VariableList::iterator it = other.returns->begin(); it!= other.returns->end(); it++)
				returns->push_back((NVariableDeclaration*) (*it)->clone() );
		}

		if(other.arguments){
			arguments = new VariableList();
			for(VariableList::iterator it = other.arguments->begin(); it!= other.arguments->end(); it++)
				arguments->push_back((NVariableDeclaration*) (*it)->clone() );
		}
		add_all_children();
	}

	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		VariableList::iterator it;
		if(returns){
			for(it = returns->begin(); it!=returns->end(); it++)
				os << **it << ", ";
		}else
			os << "void ";
		os << ":: " << *id << "(";
		for(it = arguments->begin(); it!=arguments->end(); it++)
			os << **it << ", ";
      		os << ") {\n";
		sTabs++;
		os << *block;	
 		sTabs--;
		os << "}\n";
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

	Node* clone(){
		return new NFunctionDeclaration(*this);
	}
};

#endif
