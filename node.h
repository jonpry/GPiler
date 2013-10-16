#include <iostream>
#include <vector>
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

typedef std::vector<NStatement*> StatementList;
typedef std::vector<NExpression*> ExpressionList;
typedef std::vector<NVariableDeclaration*> VariableList;
typedef std::vector<NIdentifier*> IdList;
typedef std::vector<NMap*> MapList;

class Node {
public:
	virtual ~Node() {}
	virtual llvm::Value* codeGen(CodeGenContext& context) { }

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
};

class NDouble : public NExpression {
public:
	double value;
	NDouble(double value) : value(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Double: " << value << "\n"; }
};

class NIdentifier : public NExpression {
public:
	std::string name;
	NIdentifier(const std::string& name) : name(name) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Identifier: " << name << "\n"; }
};

class NMap : public NExpression {
public:
	NIdentifier* name;
	IdList *vars;
	NExpression* expr;

	NMap(NIdentifier* name, IdList *vars, NExpression *expr) : name(name), vars(vars), expr(expr) { }
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
};

class NMethodCall : public NExpression {
public:
	NIdentifier* id;
	ExpressionList* arguments;
	NMethodCall(NIdentifier *id, ExpressionList* arguments) : id(id), arguments(arguments) { }
	NMethodCall(NIdentifier *id) : id(id) { }
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
	NPipeLine(NIdentifier *src, NIdentifier *dest, MapList *chain) : src(src), chain(chain), dest(dest) { }
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
	NBinaryOperator(NExpression *lhs, int op, NExpression *rhs) : lhs(lhs), rhs(rhs), op(op) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);

	void print(ostream& os) { 
		os << "Binary Op: ";
		switch(op){
			case TPLUS:	os << "+\n"; break;
			case TMINUS: 	os << "-\n"; break;
			case TMUL:	os << "*\n"; break;
			case TDIV:	os << "/\n"; break;
			default:	os << "unknown op\n"; break; 
		}
		sTabs++;
		os << *lhs;
		os << *rhs;
		sTabs--;
	}
};

class NAssignment : public NExpression {
public:
	NIdentifier *lhs;
	NExpression *rhs;
	NAssignment(NIdentifier *lhs, NExpression *rhs) : lhs(lhs), rhs(rhs) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Assignment: " << lhs << "\n"; }
};

class NBlock : public NExpression {
public:
	StatementList statements;
	ExpressionList expressions;
	NBlock() { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		os << "Block: \n"; 
		sTabs++;
		for(StatementList::iterator it = statements.begin(); it!= statements.end(); it++){
			os << **it;
		}
		for(ExpressionList::iterator it = expressions.begin(); it!= expressions.end(); it++){
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
	type(type), id(id), arguments(arguments), block(block) { }
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
};
