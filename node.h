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

#undef NDEBUG
#include <assert.h>
#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <typeinfo>
#include <llvm/IR/Value.h>

class Node;
class NBlock;
class NIdentifier;
class NType;
class NVariableDeclaration;
class NPipeLine;
class NMap;
class NFunctionDeclaration;
class NAssignment;

#include "parser.hpp"

#define MAX(a,b) (a>b?a:b)

using namespace std;

class CodeGenContext;
class NVariableDeclaration;

struct GType {
	int type;
	int length;
	int isArray;
	int isPointer;
	GType(int type, int length, int array) : type(type), length(length), isArray(array), isPointer(0) {}
	GType(){}

	void print();
	NType* toNode();
}; 

typedef list<NVariableDeclaration*> VariableList;
typedef list<NIdentifier*> IdList;
typedef list<NMap*> MapList;
typedef list<Node*> NodeList;
typedef list<NFunctionDeclaration*> FunctionList;
typedef list<NAssignment*> AssignmentList;
typedef list<NType*> TypeList;
typedef list<GType> GTypeList;

#define INT_TYPE 1
#define FLOAT_TYPE 2
#define BOOL_TYPE 3
#define VOID_TYPE 4

GTypeList promoteType(GTypeList ltype, GTypeList rtype);
int isCmp(int op);

class Node {
public:
	Node(){}
	//Copy constructor
	Node(const Node& other){
		//None of Node members are valid in deep copy, so do nothing
	}	
	virtual ~Node() {}
	virtual llvm::Value* codeGen(CodeGenContext& context) { 
		cout << "Unknown Codegen: " << typeid(*this).name() << "\n";
		exit(-1);
	}

	virtual llvm::Value* declGen(CodeGenContext& context) {
		cout << "Unknown declgen: " << typeid(*this).name() << "\n";
		exit(-1);
	}

	NodeList children;
	Node *parent;

	virtual void print(ostream& os) { 
		os << "Unknown Node: " << typeid(*this).name() << "\n"; 
		exit(-1);
	}

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

	void add_node_list(NodeList *exprs){
		if(!exprs)
			return;
		for(NodeList::iterator it = exprs->begin(); it != exprs->end(); it++){
			add_child((Node*)*it);
		}
	}

	void add_id_list(IdList *ids){
		if(!ids)
			return;
		for(IdList::iterator it = ids->begin(); it != ids->end(); it++){
			add_child((Node*)*it);
		}
	}

	void add_map_list(MapList *maps){
		if(!maps)
			return;
		for(MapList::iterator it = maps->begin(); it != maps->end(); it++){
			add_child((Node*)*it);
		}
	}

	virtual Node* clone() { cout << "Unknown clone: " << typeid(*this).name() << "\n"; return 0; }

	virtual GTypeList GetType(map<std::string, GTypeList> &locals) { 
		cout << "Unknown type: " <<  typeid(*this).name() << "\n"; 
		exit(-1);
		return GTypeList(); 
	}
	virtual GTypeList GetType(map<std::string, GTypeList> &locals, GTypeList* ptypes, int* found, Node* exp) { 
		cout << "Unknown type2: " <<  typeid(*this).name() << "\n";
		exit(-1); 
		return GTypeList(); 
	}

	virtual GTypeList GetType(){
		map<std::string, GTypeList> locals;
		return GetType(locals);
	}

	virtual void GetIdRefs(IdList &list) { 
		cout << "Unknown idref: " <<  typeid(*this).name() << "\n"; 
		exit(-1);
	}

	static int sTabs;  
};

class NInteger : public Node {
public:
	long long value;
	NInteger(long long value) : value(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << value; }
	GTypeList GetType(map<std::string, GTypeList> &locals) { return GTypeList{GType(INT_TYPE,32,0)};}

	Node* clone() { return new NInteger(*this); }
	void GetIdRefs(IdList &list) {}
};

class NDouble : public Node {
public:
	double value;
	NDouble(double value) : value(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << value; }
	GTypeList GetType(map<std::string, GTypeList> &locals) { return GTypeList{GType(FLOAT_TYPE,64,0)};}

	Node* clone() { return new NDouble(*this); }
	void GetIdRefs(IdList &list) {}
};

// name for a variable or function which is unique in its scope.
class NIdentifier : public Node {
public:
	std::string name;
	NIdentifier(const std::string& name) : name(name) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << name; }
	GTypeList GetType(map<std::string, GTypeList> &locals);
	void GetIdRefs(IdList &list) { list.push_back(this); }

	Node* clone(){
		return new NIdentifier(*this);
	}

	GTypeList GetType(map<std::string, GTypeList> &locals, GTypeList* ptype, int *found, Node* exp) { 
		if(exp==this){
			*ptype = GetType(locals);
			*found=1;
			return *ptype;
		}
		return GetType(locals);
	}
};

class NMap : public Node {
public:
	NIdentifier* name, *input;
	IdList *vars;
	NodeList* exprs;
	string anon_name;

	NMap(NIdentifier* name, IdList *vars, NodeList *exprs) : name(name), input(0), vars(vars), exprs(exprs) { 
		add_all_children();
	}
	~NMap(){
		delete vars;
		delete exprs;
	}

	void add_all_children(){
		if(name)
			add_child(name);
		if(input)
			add_child(input);
		add_id_list(vars);
		add_node_list(exprs);
	}

	//Copy constructor
	NMap(const NMap &other){
		name = 0; input = 0; vars = 0; exprs = 0;
		anon_name = other.anon_name;
		if(other.name)
			name = (NIdentifier*)other.name->clone();
		if(other.input)
			input = (NIdentifier*)other.input->clone();
		if(other.exprs){
			exprs = new NodeList();
			for(NodeList::iterator it = other.exprs->begin(); it!=other.exprs->end(); it++){
				exprs->push_back((Node*)(*it)->clone());
			}
		}
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
		os << ": ";
		for(NodeList::iterator it = exprs->begin(); it!=exprs->end(); it++){ 	
			os << **it << ", ";
		}
		os << ")";
		
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
	int isPointer;
	NType(const std::string& name, int isArray) : NIdentifier(name), isArray(isArray), isPointer(0) { }	
//	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		if(isPointer)
			os << "*";
		if(isArray)
			os << "[";
		os << name; 
		if(isArray)
			os << "]";
	}
	GTypeList GetType(map<std::string, GTypeList> &locals);

	Node* clone(){ return new NType(*this); }
};

class NMethodCall : public Node {
public:
	NIdentifier* id;
	NodeList* arguments;
	NMethodCall(NIdentifier *id, NodeList* arguments) : id(id), arguments(arguments) { 
		add_child(id);
		add_node_list(arguments);
	}
	NMethodCall(NIdentifier *id) : id(id) {
		add_child(id);
	}
	~NMethodCall() {
		delete arguments;
	}

	virtual llvm::Value* codeGen(CodeGenContext& context);

	void GetIdRefs(IdList &list) { 
		for(NodeList::iterator it = arguments->begin(); it!= arguments->end(); it++){
			(*it)->GetIdRefs(list);
		}
	}

	GTypeList GetType(map<std::string, GTypeList> &locals){
//		cout << id->name << "\n";	
		if(locals.find(id->name) == locals.end()){
			cout << id->name << "\n";
			assert(0);
		}
		GTypeList types = locals[id->name];
		assert(types.size());
//		cout << types.size();
		return types;
	}

	GTypeList GetType(map<std::string, GTypeList> &locals, GTypeList* ptypes, int* found, Node* exp){
		GTypeList types = GetType(locals);
		if(exp == this){
			*found = 1;
			*ptypes = types;
			return *ptypes;
		}

		return types;
	}

	void AddArgumentFront(Node *node){
		add_child(node);
		arguments->push_front(node);
	}

	void print(ostream& os) { 
//		os << "MethodCall:\n";
//		sTabs++;
		os << *id << "(";
		NodeList::iterator it;
		for(it = arguments->begin(); it!=arguments->end(); it++)
			os << ** it << ", ";
		os << ")";
//		sTabs--;
	}
};

class NZip : public Node {
public:
	IdList *src;
	NZip(IdList *src) : src(src) { }

	void print(ostream& os) { 
		os << "zip(";
		IdList::iterator it;
		for(it = src->begin(); it!=src->end(); it++)
			os << ** it << ", ";
		os << ") ";
	}

	void GetIdRefs(IdList &list) { copy(src->begin(), src->end(), back_inserter(list)); }
};

class NRef: public Node {
public:
	NIdentifier* exp;
	NRef(NIdentifier* exp) : exp(exp) {
		add_child(exp);
	}

	void print(ostream& os){
		os << "&" << *exp;
	}

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NPipeLine : public Node {
public:
	IdList *dest, *src;
	MapList *chain;
	NPipeLine(IdList *src, IdList *dest, MapList *chain) : dest(dest), src(src), chain(chain) {
		add_id_list(src);
		add_id_list(dest);
		add_map_list(chain);
 	}
	~NPipeLine(){
		delete chain;
		delete dest;
		delete src;
	}

//	virtual llvm::Value* codeGen(CodeGenContext& context);

	void print(ostream& os) { 
		for(IdList::iterator it = src->begin(); it!=src->end(); it++){
			os << **it << ", ";
		}
		for(MapList::iterator it = chain->begin(); it!=chain->end(); it++){
			os << " :: " << **it;
		}
		os << " > ";

		for(IdList::iterator it = dest->begin(); it!=dest->end(); it++){
			os << **it << ", ";
		}

	}
};

class NBinaryOperator : public Node {
public:
	int op;
	Node *lhs, *rhs;
	NBinaryOperator(Node *lhs, int op, Node *rhs) : op(op), lhs(lhs), rhs(rhs) { 
		add_all_children();
	}
	//Copy constructor
	NBinaryOperator(const NBinaryOperator &other){
		op = other.op;
		lhs = other.lhs->clone();
		rhs = other.rhs->clone();
		add_all_children();
	}

	void add_all_children(){
		add_child(lhs);
		add_child(rhs);
	}

	virtual llvm::Value* codeGen(CodeGenContext& context);

	GTypeList GetType(map<std::string, GTypeList> &locals, GTypeList* ptype, int *found, Node* exp) { 
		if(exp==this){
			*ptype = GetType(locals);
			*found=1;
			return *ptype;
		}

		GTypeList ret = rhs->GetType(locals,ptype,found,exp);
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

	GTypeList GetType(map<std::string, GTypeList> &locals);

	void GetIdRefs(IdList &list) { lhs->GetIdRefs(list); rhs->GetIdRefs(list); }

	Node* clone(){ return new NBinaryOperator(*this); }
};

class NSelect : public Node {
public:
	Node *pred, *yes, *no;
	NSelect(Node *pred, Node *yes, Node *no) : pred(pred), yes(yes), no(no) { 
		add_child(pred);
		add_child(yes);
		add_child(no);
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);


	GTypeList GetType(map<std::string, GTypeList> &locals, GTypeList* ptype, int *found, Node* exp) { 
		if(exp==this){
			*ptype = GetType(locals);
			*found=1;
			return *ptype;
		}

		GTypeList ret = yes->GetType(locals,ptype,found,exp);
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

	void GetIdRefs(IdList &list) { 
		pred->GetIdRefs(list);
		yes->GetIdRefs(list);
		no->GetIdRefs(list); 
	}

	GTypeList GetType(map<std::string, GTypeList> &locals);

	void print(ostream& os) { 
		os << *pred << "?";
		os << *yes << ":";
		os << *no;
	}
};

class NArrayRef : public NIdentifier {
public:
	NIdentifier *index;
	NArrayRef(NIdentifier *array, NIdentifier *index) : NIdentifier(*array), index(index) { 
		add_child(index);
	}
	//Copy constructor
	NArrayRef(const NArrayRef &other) : NIdentifier(other) {
		index = (NIdentifier*)other.index->clone();
	}

	virtual llvm::Value* codeGen(CodeGenContext& context);
	llvm::Value* store(CodeGenContext& context, llvm::Value* rhs);

	void print(ostream& os) { 
		os << name << "[" << *index << "]";
	}

	Node* clone(){
		return new NArrayRef(*this);
	}
};

class NAssignment : public Node {
public:
	IdList *lhs;
	Node *rhs;
	NArrayRef *array;
	int isReturn;
	NAssignment(IdList *lhs, Node *rhs) : lhs(lhs), rhs(rhs), array(0), isReturn(0) {add_all_children(); }
	//Copy constructor
	NAssignment(const NAssignment &other){
		isReturn = other.isReturn;
		lhs = 0; rhs = 0;
		if(other.lhs){
			lhs = new IdList();
			for(IdList::iterator it = other.lhs->begin(); it != other.lhs->end(); it++){
				lhs->push_back((NIdentifier*)(*it)->clone());
			}
		}
		if(other.rhs)
			rhs = other.rhs->clone();
	}
	~NAssignment(){
		delete lhs;
	}

	virtual llvm::Value* codeGen(CodeGenContext& context);

	GTypeList GetType(map<std::string, GTypeList> &locals, GTypeList* ptype, int *found, Node* exp) { 
		if(exp==this){
			*ptype = rhs->GetType(locals);
			*found=1;
			return *ptype;
		}

		GTypeList ret = rhs->GetType(locals,ptype,found,exp);
		if(*found)
			return ret;

		{
			//TODO: assignment destination can be scalar with vector expression
			if(lhs->size() == 1){
				locals[(*lhs->begin())->name] = ret;
			}else{
				IdList::iterator it;
				GTypeList::iterator it2;
				for(it = lhs->begin(), it2 = ret.begin(); it != lhs->end() && it2 != ret.end(); it++, it2++)
					locals[(*it)->name] = GTypeList{*it2};
				if(it!=lhs->end() || it2 != ret.end()){
					cout << "Assignment vector length mismatch!\n";
					exit(-1);
				}
			}
		}
		return ret;	
	}

	void GetIdRefs(IdList &list) { rhs->GetIdRefs(list); }

	void add_all_children(){
		add_id_list(lhs);
		if(rhs)
			add_child(rhs);
	}

	void SetExpr(Node *node){
		children.remove(rhs);
		add_child(node);
		rhs = node;
	}

	void SetArray(NArrayRef *in){
		array = in;
		add_child(in);
		if(lhs){
			for(IdList::iterator it=lhs->begin(); it!=lhs->end(); it++){
				children.remove(*it);
			}
			delete lhs;
			lhs = 0;
		}
	}

	void print(ostream& os) 
	{ 
//		os << "Assignment: return: " << isReturn << "\n";
//		sTabs++;
		if(lhs){
			for(IdList::iterator it = lhs->begin(); it != lhs->end(); it++){
				os << **it << ", ";
			}
		}
		if(array)
			os << *array << " ";
		os << " = ";
		os << *rhs; 
//		sTabs--;
	}

	Node* clone() { return new NAssignment(*this); }
};

class NBlock : public Node {
public:
	NBlock() { }
	//Copy constructor
	NBlock(const NBlock &other){
		for(NodeList::const_iterator it=other.children.begin(); it!=other.children.end(); it++){
			add_child( (*it)->clone() );
		}
	}

	virtual llvm::Value* codeGen(CodeGenContext& context);
	virtual llvm::Value* declGen(CodeGenContext& context);

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
	GTypeList GetType(map<std::string, GTypeList> &locals, GTypeList* ptype, int *found, Node* exp) { 
		NodeList::iterator it;
		for(it = children.begin(); it!= children.end(); it++){
			GTypeList ret = ((*it))->GetType(locals,ptype,found,exp);
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

class NVariableDeclaration : public Node {
public:
 	TypeList *types;
	NIdentifier *id;
	Node *assignmentExpr;
	NVariableDeclaration(TypeList *types, NIdentifier *id) : types(types), id(id), assignmentExpr(0) { add_all_children(); }
	NVariableDeclaration(TypeList *types, NIdentifier *id, Node *assignmentExpr) : types(types), id(id), assignmentExpr(assignmentExpr) {
		add_all_children();
	}
	//Copy constructor
	NVariableDeclaration(const NVariableDeclaration &other){
		id=0;
		assignmentExpr=0;
		types = 0;
		if(other.types){
			types = new TypeList();
			for(TypeList::iterator it = other.types->begin(); it != other.types->end(); it++){
				types->push_back((NType*)(*it)->clone());
			}
		}
		if(other.id)
			id = (NIdentifier*)other.id->clone();
		if(other.assignmentExpr)
			assignmentExpr = other.assignmentExpr->clone();

		add_all_children();
	}
	~NVariableDeclaration(){
		delete types;
	}

	void add_all_children(){
		if(types){
			for(TypeList::iterator it = types->begin(); it != types->end(); it++){
				add_child(*it);
			}
		}
		if(id)
			add_child(id);
		if(assignmentExpr)
			add_child(assignmentExpr);
	}

	llvm::Value* codeGen(CodeGenContext& context);
	void GetIdRefs(IdList &list) { if(assignmentExpr) assignmentExpr->GetIdRefs(list); }
	void print(ostream& os) { 
//		os << "Variable decl:\n"; 
//		sTabs++;
		for(TypeList::iterator it = types->begin(); it != types->end(); it++){
			os << **it << ", ";
		}
		if(id)
			os << " : " << *id;
		if(assignmentExpr)
			os << " = " << *assignmentExpr;
//		sTabs--;
	}

	void SetType(NType* new_type){
		if(types){
			for(TypeList::iterator it = types->begin(); it != types->end(); it++){
				children.remove(*it);
			}
		}else
			types = new TypeList();
		add_child(new_type);
		types->push_back(new_type);
	}

	void SetExpr(Node* expr){
		children.remove(assignmentExpr);
		if(expr)
			add_child(expr);
		assignmentExpr = expr;
	}

	GTypeList GetType(map<std::string, GTypeList> &locals){ 
		GTypeList ret;
		if(types){
			for(TypeList::iterator it=types->begin(); it!=types->end(); it++){
				ret.splice(ret.end(), (*it)->GetType(locals));
			}
		}
		return ret; 
	}

	Node* clone() { return new NVariableDeclaration(*this); }
};

class NLoop : public Node {
public:
	NBlock *block;
	NLoop(NBlock *block) : block(block) {}		
	void print(ostream& os) { 
		os << "for(something){\n";
		sTabs++;
		if(block)
			os << *block;
		sTabs--;
		for(int i=0; i < sTabs; i++)
			os << "\t";
		os << "}";
	}
};

class NFunctionDeclaration : public Node {
public:
	NIdentifier *id;
	VariableList *returns, *arguments;
	NBlock *block;
	int isGenerated;
	NFunctionDeclaration(VariableList* returns, NIdentifier* id, VariableList* arguments, NBlock *block) :
			id(id), returns(returns), arguments(arguments), block(block), isGenerated(0) { 
		add_all_children();
	}
	~NFunctionDeclaration(){
		delete returns;
		delete arguments;
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
	virtual llvm::Value* declGen(CodeGenContext& context);

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

	void AddArgument(NVariableDeclaration *decl){
		if(!arguments)
			arguments = new VariableList();
		add_child(decl);
		arguments->push_back(decl);
	}

	void AddReturn(NVariableDeclaration *decl){
		if(!returns)
			returns = new VariableList();
		add_child(decl);
		returns->push_back(decl);
	}

	int isScalar(){
		for(VariableList::iterator it = returns->begin(); it!= returns->end(); it++){
			for(TypeList::iterator it2 = (*it)->types->begin(); it2 != (*it)->types->end(); it2++)
				if((*it2)->isArray)
					return 0;
		}
		for(VariableList::iterator it = arguments->begin(); it!= arguments->end(); it++)
			for(TypeList::iterator it2 = (*it)->types->begin(); it2 != (*it)->types->end(); it2++)	
				if((*it2)->isArray)
					return 0;
		return 1;
	}

	Node* clone(){
		return new NFunctionDeclaration(*this);
	}

	void ClearReturns(){
		if(!returns)
			return;
		for(VariableList::iterator it=returns->begin(); it!=returns->end(); it++){
			children.remove(*it);
		}
		returns->clear();
		delete returns;
		returns = 0;
	}


	void ClearArguments(){
		if(!arguments)
			return;
		for(VariableList::iterator it=arguments->begin(); it!=arguments->end(); it++){
			children.remove(*it);
		}
		arguments->clear();
		delete arguments;
		arguments = 0;
	}

	GTypeList GetType(map<std::string, GTypeList> &locals){
		GTypeList ret;
		for(VariableList::iterator it=returns->begin(); it!=returns->end(); it++){
			ret.splice(ret.end(), (*it)->GetType(locals));
		}
		return ret;
	}
};

#endif
