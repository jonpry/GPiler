#include "node.h"
#include "codegen.h"
#include "parser.hpp"

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
		ret = Type::getInt32Ty(getGlobalContext());
	}else if (type->name.compare("void") == 0) {
		ret = Type::getVoidTy(getGlobalContext());
	} else cout << "Error unknown type\n";

	if(type->isArray){
		ret = PointerType::get(ret,1);//1 is global address space
	}

	return ret;
}

GType NType::GetType(CodeGenContext& context){
	GType ret;
	if (name.compare("int") == 0 || name.compare("int32") == 0) {
		ret.type = INT_TYPE; ret.length = 32;
	}else if (name.compare("int64") == 0) {
		ret.type = INT_TYPE; ret.length = 64;
	}else if (name.compare("double") == 0) {
		ret.type = FLOAT_TYPE; ret.length = 32;
	}else if (name.compare("float") == 0) {
		ret.type = FLOAT_TYPE; ret.length = 64;
	}else if (name.compare("int16") == 0) {
		ret.type = INT_TYPE; ret.length = 32;
	}else if (name.compare("int8") == 0) {
		ret.type = INT_TYPE; ret.length = 8;
	}else if (name.compare("bool") == 0) {
		ret.type = INT_TYPE; ret.length = 32;
	}else if (name.compare("void") == 0) {
		ret.type = VOID_TYPE; ret.length = 0;
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
	return new LoadInst(context.locals()[name]->value, "", false, context.currentBlock());
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

GType NIdentifier::GetType(CodeGenContext& context) { 
	return context.locals()[name]->type;
}

Value* NBinaryOperator::codeGen(CodeGenContext& context)
{
	std::cout << "Creating binary operation " << op << endl;
	Instruction::BinaryOps instr;

	GType ltype = lhs->GetType(context);
	GType rtype = lhs->GetType(context);

	//TODO: argument promotion
	if(ltype.type == FLOAT_TYPE || rtype.type == FLOAT_TYPE){
		switch (op) {
			case TPLUS: instr = Instruction::FAdd; break;
			case TMINUS: instr = Instruction::FSub; break;
			case TMUL: instr = Instruction::FMul; break;
			case TDIV: instr = Instruction::FDiv; break;
			default: cout << "Unknown op\n"; return 0;
			/* TODO comparison */
		}
	}else{
		switch (op) {
			case TPLUS: instr = Instruction::Add; break;
			case TMINUS: instr = Instruction::Sub; break;
			case TMUL: instr = Instruction::Mul; break;
			case TDIV: instr = Instruction::SDiv; break;
			default: cout << "Unknown op\n"; return 0;
			/* TODO comparison */
		}
	}

	return BinaryOperator::Create(instr, lhs->codeGen(context),
		rhs->codeGen(context), "", context.currentBlock());
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
	return new StoreInst(rhs->codeGen(context), context.locals()[lhs->name]->value, false, context.currentBlock());
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
	context.locals()[id->name] = new GValue(alloc,type->GetType(context));
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

	Value* idx = new LoadInst(context.locals()[index->name]->value, "", false, context.currentBlock());
	Value* ptr = new LoadInst(context.locals()[array->name]->value, "", false, context.currentBlock());
	Value* gep = GetElementPtrInst::Create(ptr, ArrayRef<Value*>(idx), "", context.currentBlock());
	//cout << "Generated\n";
//	Value* ld1 = new LoadInst(gep, "", false, context.currentBlock());
	return new LoadInst(gep, "", false, context.currentBlock());
}

Value* NArrayRef::store(CodeGenContext& context, Value* rhs){
	std::cout << "Creating array store " << array->name << " " << index->name << endl;

	Value* idx = new LoadInst(context.locals()[index->name]->value, "", false, context.currentBlock());
	Value* ptr = new LoadInst(context.locals()[array->name]->value, "", false, context.currentBlock());
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
		new StoreInst((Value*)AI, context.locals()[ (*it)->id->name]->value, false, context.currentBlock());
  	}

	block->codeGen(context);
	if(type->name.compare("void")==0)
		ReturnInst::Create(getGlobalContext(), bblock);

	context.popBlock();
	std::cout << "Creating function: " << id->name << endl;
	return function;
}
