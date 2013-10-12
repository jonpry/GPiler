#include <iostream>
#include <vector>
#include <llvm/Value.h>

class Node;
class NBlock;
class NExpression;
class NStatement;
class NIdentifier;
class NVariableDeclaration;

#include "parser.hpp"

using namespace std;

class CodeGenContext;
class NStatement;
class NExpression;
class NVariableDeclaration;

typedef std::vector<NStatement*> StatementList;
typedef std::vector<NExpression*> ExpressionList;
typedef std::vector<NVariableDeclaration*> VariableList;

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

class NMethodCall : public NExpression {
public:
	const NIdentifier& id;
	ExpressionList arguments;
	NMethodCall(const NIdentifier& id, ExpressionList& arguments) :
	id(id), arguments(arguments) { }
	NMethodCall(const NIdentifier& id) : id(id) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);

	void print(ostream& os) { 
		os << "MethodCall:\n";
		sTabs++;
		os << (Node&)id;
		ExpressionList::iterator it;
		for(it = arguments.begin(); it!=arguments.end(); it++)
			os << ** it;
		sTabs--;
	}
};

class NBinaryOperator : public NExpression {
public:
	int op;
	NExpression& lhs;
	NExpression& rhs;
	NBinaryOperator(NExpression& lhs, int op, NExpression& rhs) :
	lhs(lhs), rhs(rhs), op(op) { }
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
		os << lhs;
		os << rhs;
		sTabs--;
	}
};

class NAssignment : public NExpression {
public:
	NIdentifier& lhs;
	NExpression& rhs;
	NAssignment(NIdentifier& lhs, NExpression& rhs) :
	lhs(lhs), rhs(rhs) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { os << "Assignment: " << lhs << "\n"; }
};

class NBlock : public NExpression {
public:
	StatementList statements;
	NBlock() { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		os << "Block: \n"; 
		StatementList::iterator it;
		sTabs++;
		for(it = statements.begin(); it!= statements.end(); it++){
			os << **it;
		}
		sTabs--;
	}
};

class NExpressionStatement : public NStatement {
public:
	NExpression& expression;
	NExpressionStatement(NExpression& expression) :
	expression(expression) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		os << "Expression statement:\n"; 
		sTabs++;
		os << expression;
		sTabs--;
	}
};

class NVariableDeclaration : public NStatement {
public:
	const NIdentifier& type;
	NIdentifier& id;
	NExpression *assignmentExpr;
	NVariableDeclaration(const NIdentifier& type, NIdentifier& id) :
	type(type), id(id) { }
	NVariableDeclaration(const NIdentifier& type, NIdentifier& id, NExpression *assignmentExpr) :
	type(type), id(id), assignmentExpr(assignmentExpr) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		os << "Variable decl:\n"; 
		sTabs++;
		os << (Node&)type;
		os << id;
		if(assignmentExpr)
			os << *assignmentExpr;
		sTabs--;
	}
};

class NFunctionDeclaration : public NStatement {
public:
	const NIdentifier& type;
	const NIdentifier& id;
	VariableList arguments;
	NBlock& block;
	NFunctionDeclaration(const NIdentifier& type, const NIdentifier& id,
	const VariableList& arguments, NBlock& block) :
	type(type), id(id), arguments(arguments), block(block) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
	void print(ostream& os) { 
		os << "Function decl:\n"; 
		sTabs++;
		os << (Node&)type << (Node&)id;
		VariableList::iterator it;
		for(it = arguments.begin(); it!=arguments.end(); it++)
			os << **it;
		os << block;	
 		sTabs--;
	}
};
