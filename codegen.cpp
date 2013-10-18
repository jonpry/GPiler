#include "node.h"
#include "codegen.h"
#include "parser.hpp"

using namespace std;

/* Compile the AST into a module */
void CodeGenContext::generateCode(NBlock& root)
{
	std::cout << "Generating code...\n";

	/* Create the top level interpreter function to call as entry */
//	vector<Type*> argTypes;
//	FunctionType *ftype = FunctionType::get(Type::getVoidTy(getGlobalContext()), makeArrayRef(argTypes), false);
//	mainFunction = Function::Create(ftype, GlobalValue::InternalLinkage, "main", module);
//	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", mainFunction, 0);

	/* Push a new variable/block context */
//	pushBlock(bblock);
	root.codeGen(*this); /* emit bytecode for the toplevel block */
//	ReturnInst::Create(getGlobalContext(), bblock);
//	popBlock();

	/* Print the bytecode in a human-readable format
	to see if our program compiled properly
	*/
	std::cout << "Code is generated.\n";
	PassManager pm;
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
	}
	else if (type->name.compare("double") == 0) {
		ret = Type::getDoubleTy(getGlobalContext());
	} else ret = Type::getVoidTy(getGlobalContext());

	if(type->isArray){
		ret = PointerType::get(ret,1);//1 is global address space
	}

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

Value* NBinaryOperator::codeGen(CodeGenContext& context)
{
	std::cout << "Creating binary operation " << op << endl;
	Instruction::BinaryOps instr;
	switch (op) {
		case TPLUS: instr = Instruction::Add; goto math;
		case TMINUS: instr = Instruction::Sub; goto math;
		case TMUL: instr = Instruction::Mul; goto math;
		case TDIV: instr = Instruction::SDiv; goto math;

		/* TODO comparison */
	}

	return NULL;
	math:
	return BinaryOperator::Create(instr, lhs->codeGen(context),
		rhs->codeGen(context), "", context.currentBlock());
}

Value* NAssignment::codeGen(CodeGenContext& context)
{
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
	for (StatementList::const_iterator it = statements.begin(); it != statements.end(); it++) {
		std::cout << "Generating code for " << typeid(**it).name() << endl;
		last = (**it).codeGen(context);
	}
	for (ExpressionList::const_iterator it = expressions.begin(); it != expressions.end(); it++) {
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
	if (assignmentExpr != NULL) {
		NAssignment assn(id, assignmentExpr);
		assn.codeGen(context);
	}
	return alloc;
}

Value* NArrayRef::codeGen(CodeGenContext& context)
{
	//TODO:
	std::cout << "Creating array reference " << array->name << " " << index->name << endl;

	std::map<std::string, Value*>::iterator it;
	for(it = context.locals().begin(); it!= context.locals().end(); it++){
		std::cout << (*it).first << " " << (*it).second << "\n";
	}
	char gep_name[64];
	sprintf(gep_name, "gep.%s.%s", array->name.c_str(), index->name.c_str());

	Value* gep = GetElementPtrInst::Create(context.locals()[array->name], ArrayRef<Value*>(context.locals()[index->name]),gep_name,context.currentBlock());
	cout << "Generated\n";
	Value* ld1 = new LoadInst(gep, "", false, context.currentBlock());
	return new LoadInst(ld1, "", false, context.currentBlock());
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
	FunctionType *ftype = FunctionType::get(typeOf(type), makeArrayRef(argTypes), false);
	Function *function = Function::Create(ftype, GlobalValue::InternalLinkage, id->name.c_str(), context.module);
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
	if(type->name.compare("void")==0)
		ReturnInst::Create(getGlobalContext(), bblock);

	context.popBlock();
	std::cout << "Creating function: " << id->name << endl;
	return function;
}
