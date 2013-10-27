/* 
GPiler - codegen.cpp
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

#include "llvm/Transforms/IPO.h"

using namespace std;

/* Compile the AST into a module */
void CodeGenContext::generateCode(NBlock& root)
{
	std::cout << "Generating code...\n";

	root.codeGen(*this); /* emit bytecode for the toplevel block */

	/* Print the bytecode in a human-readable format
	to see if our program compiled properly
	*/
	std::cout << "Code is generated.\n";
	PassManager pm;
	pm.add( createFunctionInliningPass(275));
	pm.add(createPrintModulePass(&outs()));
	pm.run(*module);
}

/* Executes the AST by running the main function */
GenericValue CodeGenContext::runCode() {
	std::cout << "Running code...\n";
	ExecutionEngine *ee = EngineBuilder(module).create();
	vector<GenericValue> noargs;
	GenericValue v = ee->runFunction(mainFunction, noargs);
	std::cout << "Code was run.\n";
	return v;
}

/* Returns an LLVM type based on the identifier */
static Type *typeOf(const NType* type)
{
	Type* ret=0;
	if (type->name.compare("int") == 0 || type->name.compare("int32") == 0) {
		ret = Type::getInt32Ty(getGlobalContext());
	}else if (type->name.compare("int64") == 0) {
		ret = Type::getInt64Ty(getGlobalContext());
	}else if (type->name.compare("double") == 0) {
		ret = Type::getDoubleTy(getGlobalContext());
	}else if (type->name.compare("float") == 0) {
		ret = Type::getFloatTy(getGlobalContext());
	}else if (type->name.compare("int16") == 0) {
		ret = Type::getInt16Ty(getGlobalContext());
	}else if (type->name.compare("int8") == 0) {
		ret = Type::getInt8Ty(getGlobalContext());
	}else if (type->name.compare("bool") == 0) {
		ret = Type::getInt1Ty(getGlobalContext());
	}else if (type->name.compare("void") == 0) {
		ret = Type::getVoidTy(getGlobalContext());
	} else cout << "Error unknown type: " << type->name << "\n";

	if(type->isArray){
		ret = PointerType::get(ret,1);//1 is global address space
	}

	return ret;
}

static Type *typeOf(const NVariableDeclaration *decl){
	if(!decl)
		return Type::getVoidTy(getGlobalContext());
	return typeOf(decl->type);
}

/* Returns an LLVM type based on GType */
static Type *typeOf(GType type)
{
	Type* ret=0;
	if (type.type == BOOL_TYPE){
		ret = Type::getInt1Ty(getGlobalContext());
	}else if (type.type == INT_TYPE && type.length==32) {
		ret = Type::getInt32Ty(getGlobalContext());
	}else if (type.type == INT_TYPE && type.length==64) {
		ret = Type::getInt64Ty(getGlobalContext());
	}else if (type.type == FLOAT_TYPE && type.length==64) {
		ret = Type::getDoubleTy(getGlobalContext());
	}else if (type.type == FLOAT_TYPE && type.length==32) {
		ret = Type::getFloatTy(getGlobalContext());
	}else if (type.type == INT_TYPE && type.length==16) {
		ret = Type::getInt16Ty(getGlobalContext());
	}else if (type.type == INT_TYPE && type.length==8) {
		ret = Type::getInt8Ty(getGlobalContext());
	}else if (type.type == VOID_TYPE) {
		ret = Type::getVoidTy(getGlobalContext());
	} else cout << "Error unknown type\n";

	return ret;
}

/* -- Code Generation -- */

Value* NInteger::codeGen(CodeGenContext& context)
{
	std::cout << "Creating integer: " << value << endl;
	return ConstantInt::get(Type::getInt32Ty(getGlobalContext()), value, true);
}

Value* NDouble::codeGen(CodeGenContext& context)
{
	std::cout << "Creating double: " << value << endl;
	return ConstantFP::get(Type::getDoubleTy(getGlobalContext()), value);
}

Value* NIdentifier::codeGen(CodeGenContext& context)
{
	std::cout << "Creating identifier reference: " << name << endl;
	if (context.locals().find(name) == context.locals().end()) {
		std::cerr << "undeclared variable " << name << endl;
		return NULL;
	}
	return new LoadInst(context.locals()[name], "", false, context.currentBlock());
}

Value* NMethodCall::codeGen(CodeGenContext& context)
{
	Function *function = context.module->getFunction(id->name.c_str());
	if (function == NULL) {
		std::cerr << "no such function " << id->name << endl;	
	}
	std::vector<Value*> args;
	ExpressionList::const_iterator it;
	for (it = arguments->begin(); it != arguments->end(); it++) {
		args.push_back((**it).codeGen(context));
	}
	CallInst *call = CallInst::Create(function, makeArrayRef(args), "", context.currentBlock());
	std::cout << "Creating method call: " << id->name << endl;
	return call;
}

int isCmp(int op){
	//TODO: more comparison
	switch(op){
		case TCGT: return 1;
		case TCLT: return 1;
		case TCEQ: return 1;
		case TCLE: return 1;
		case TCGE: return 1;
		case TCNE: return 1;
		default: return 0;
	}
}

void Promote(Value **lhc, Value** rhc, NExpression *lhs, NExpression *rhs,CodeGenContext& context){
	GType ltype = lhs->GetType(context.localTypes());
	GType rtype = rhs->GetType(context.localTypes());

	*lhc = lhs->codeGen(context);
	*rhc = rhs->codeGen(context);

	if(ltype.type == FLOAT_TYPE || rtype.type == FLOAT_TYPE){
		if(rtype.type != FLOAT_TYPE){
			*rhc = new SIToFPInst(*rhc,typeOf(ltype),"", context.currentBlock());
		}

		if(ltype.type != FLOAT_TYPE){
			*lhc = new SIToFPInst(*lhc,typeOf(rtype),"", context.currentBlock());
		}
	}
}

Value* NBinaryOperator::codeGen(CodeGenContext& context)
{
	std::cout << "Creating binary operation " << op << endl;
	Instruction::BinaryOps instr;
	CmpInst::Predicate pred;

	Value *lhc, *rhc;

	GType ltype = lhs->GetType(context.localTypes());
	GType rtype = rhs->GetType(context.localTypes());

	Promote(&lhc,&rhc,lhs,rhs,context);

	//TODO: argument promotion
	if(ltype.type == FLOAT_TYPE || rtype.type == FLOAT_TYPE){
		switch (op) {
			case TPLUS: instr = Instruction::FAdd; break;
			case TMINUS: instr = Instruction::FSub; break;
			case TMUL: instr = Instruction::FMul; break;
			case TDIV: instr = Instruction::FDiv; break;
			case TCGT: pred = CmpInst::Predicate::FCMP_OGT; break;
			case TCLT: pred = CmpInst::Predicate::FCMP_OLT; break;
			case TCGE: pred = CmpInst::Predicate::FCMP_OGE; break;
			case TCLE: pred = CmpInst::Predicate::FCMP_OLE; break;
			case TCEQ: pred = CmpInst::Predicate::FCMP_OEQ; break;
			case TCNE: pred = CmpInst::Predicate::FCMP_ONE; break;
			default: cout << "Unknown FP op\n"; return 0;
			/* TODO comparison */
		}
		if(isCmp(op)){
			return new FCmpInst(*context.currentBlock(), pred, lhc, rhc, "");
		}
		return BinaryOperator::Create(instr, lhc,
			rhc, "", context.currentBlock());

	}else{
		switch (op) {
			case TPLUS: instr = Instruction::Add; break;
			case TMINUS: instr = Instruction::Sub; break;
			case TMUL: instr = Instruction::Mul; break;
			case TDIV: instr = Instruction::SDiv; break;
			case TOR: instr = Instruction::Or; break;
			case TAND: instr = Instruction::And; break;
			case TLSL: instr = Instruction::Shl; break;
			case TLSR: instr = Instruction::LShr; break;
			case TCGT: pred = CmpInst::Predicate::ICMP_SGT; break;
			case TCLT: pred = CmpInst::Predicate::ICMP_SLT; break;
			case TCGE: pred = CmpInst::Predicate::ICMP_SGE; break;
			case TCLE: pred = CmpInst::Predicate::ICMP_SLE; break;
			case TCEQ: pred = CmpInst::Predicate::ICMP_EQ; break;
			case TCNE: pred = CmpInst::Predicate::ICMP_NE; break;
			default: cout << "Unknown op\n"; return 0;
		}

		if(isCmp(op)){
			return new ICmpInst(*context.currentBlock(), pred, lhc, rhc, "");
		}
		return BinaryOperator::Create(instr, lhc,
			rhc, "", context.currentBlock());
	}
}

Value* GetIntZero(){
	return ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0, true);
}

Value* GetFloatZero(){
	return ConstantFP::get(Type::getDoubleTy(getGlobalContext()), 0);
}

Value* NSelect::codeGen(CodeGenContext& context)
{
	std::cout << "Creating if operation " << endl;
	
	Value *lhc, *rhc;

	Promote(&lhc,&rhc,yes,no,context);

	GType ptype = pred->GetType(context.localTypes());
	Value* predv = pred->codeGen(context);
	if(ptype.type == INT_TYPE){
		predv = new ICmpInst(*context.currentBlock(),CmpInst::Predicate::ICMP_NE,predv,GetIntZero());
	}
	if(ptype.type == FLOAT_TYPE){
		predv = new FCmpInst(*context.currentBlock(),CmpInst::Predicate::FCMP_ONE,predv,GetFloatZero());
	}	
	
	return SelectInst::Create(predv, lhc, rhc, "", context.currentBlock());
}

Value* NAssignment::codeGen(CodeGenContext& context)
{
	if(array){
		return array->store(context,rhs->codeGen(context));
	}
	std::cout << "Creating assignment for " << lhs->name << endl;
	if (context.locals().find(lhs->name) == context.locals().end()) {
		std::cerr << "undeclared variable " << lhs->name << endl;
		return NULL;
	}
	if(isReturn)
		return ReturnInst::Create(getGlobalContext(), rhs->codeGen(context), context.currentBlock());
	return new StoreInst(rhs->codeGen(context), context.locals()[lhs->name], false, context.currentBlock());
}

Value* NBlock::codeGen(CodeGenContext& context)
{
	Value *last = NULL;
	for (NodeList::const_iterator it = children.begin(); it != children.end(); it++) {
		std::cout << "Generating code for " << typeid(**it).name() << endl;
		last = (**it).codeGen(context);
	}
	std::cout << "Creating block" << endl;
	return last;
}

Value* NVariableDeclaration::codeGen(CodeGenContext& context)
{
	std::cout << "Creating variable declaration " << type->name << " " << id->name << endl;
	AllocaInst *alloc = new AllocaInst(typeOf(type), id->name.c_str(), context.currentBlock());
	context.locals()[id->name] = alloc;
	context.localTypes()[id->name] = type->GetType(context.localTypes());
	//cout << context.locals()[id->name]->type.length;
	if (assignmentExpr != NULL) {
		NAssignment assn(id, assignmentExpr);
		assn.codeGen(context);
	}
	return alloc;
}

Value* NArrayRef::codeGen(CodeGenContext& context)
{
	std::cout << "Creating array reference " << array->name << " " << index->name << endl;

	/*std::map<std::string, Value*>::iterator it;
	for(it = context.locals().begin(); it!= context.locals().end(); it++){
		std::cout << (*it).first << " " << (*it).second << "\n";
	}*/

	Value* idx = new LoadInst(context.locals()[index->name], "", false, context.currentBlock());
	Value* ptr = new LoadInst(context.locals()[array->name], "", false, context.currentBlock());
	Value* gep = GetElementPtrInst::Create(ptr, ArrayRef<Value*>(idx), "", context.currentBlock());
	//cout << "Generated\n";
//	Value* ld1 = new LoadInst(gep, "", false, context.currentBlock());
	return new LoadInst(gep, "", false, context.currentBlock());
}

Value* NArrayRef::store(CodeGenContext& context, Value* rhs){
	std::cout << "Creating array store " << array->name << " " << index->name << endl;

	Value* idx = new LoadInst(context.locals()[index->name], "", false, context.currentBlock());
	Value* ptr = new LoadInst(context.locals()[array->name], "", false, context.currentBlock());
	Value* gep = GetElementPtrInst::Create(ptr, ArrayRef<Value*>(idx), "", context.currentBlock());
	//cout << "Generated\n";
//	Value* ld1 = new LoadInst(gep, "", false, context.currentBlock());
	return new StoreInst(rhs,gep, "", false, context.currentBlock());
}

void addKernelMetadata(llvm::Function *F) {
  llvm::Module *M = F->getParent();
  llvm::LLVMContext &Ctx = M->getContext();

  // Get "nvvm.annotations" metadata node
  llvm::NamedMDNode *MD = M->getOrInsertNamedMetadata("nvvm.annotations");

  // Create !{<func-ref>, metadata !"kernel", i32 1} node
  llvm::SmallVector<llvm::Value *, 3> MDVals;
  MDVals.push_back(F);
  MDVals.push_back(llvm::MDString::get(Ctx, "device"));
  MDVals.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1));

  // Append metadata to nvvm.annotations
  MD->addOperand(llvm::MDNode::get(Ctx, MDVals));
}
 

Value* NFunctionDeclaration::codeGen(CodeGenContext& context)
{
	vector<Type*> argTypes;
	VariableList::const_iterator it;
	for (it = arguments->begin(); it != arguments->end(); it++) {
		argTypes.push_back(typeOf((**it).type));
	}	

	FunctionType *ftype = FunctionType::get(typeOf(returns->front()), makeArrayRef(argTypes), false);
	Function *function = Function::Create(ftype, isGenerated?GlobalValue::InternalLinkage:GlobalValue::ExternalLinkage, id->name.c_str(), context.module);
	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", function, 0);

	addKernelMetadata(function);

	context.pushBlock(bblock);

	if(id->name == "main")
		context.mainFunction = function;

  	// Set names for all arguments.
	Function::arg_iterator AI;
  	for (AI = function->arg_begin(), it = arguments->begin(); it != arguments->end(); ++AI, ++it) {
		const char* name = (*it)->id->name.c_str();
    		AI->setName(name);

    	// Add arguments to variable symbol table.
    	//		context.locals()[(*it)->id.name] = AI;

		(*it)->codeGen(context);
		new StoreInst((Value*)AI, context.locals()[ (*it)->id->name], false, context.currentBlock());
  	}

	block->codeGen(context);
	if(returns->empty())
		ReturnInst::Create(getGlobalContext(), bblock);

	context.popBlock();
	std::cout << "Creating function: " << id->name << endl;
	return function;
}
