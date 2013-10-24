/* 
GPiler - codegen.h
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

#include <stack>
#include <typeinfo>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/PassManager.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
//#include <llvm/ModuleProvider.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Support/raw_ostream.h>

#include "node.h"

using namespace llvm;

GType GetType(NBlock *pb, NExpression *exp);

class NBlock;

class CodeGenBlock {
public:
	CodeGenBlock(CodeGenBlock *parent){
		if(parent){
			locals = parent->locals;
			localTypes = parent->localTypes;
		}
    	}

    	BasicBlock *block;
    	std::map<std::string, Value*> locals;
    	std::map<std::string, GType> localTypes;
};

class CodeGenContext {
    	std::stack<CodeGenBlock *> blocks;

public:
    	Function *mainFunction;
    	Module *module;
    	CodeGenContext() { module = new Module("main", getGlobalContext()); }
	~CodeGenContext() { 
		while(!blocks.empty()){
			CodeGenBlock *top = blocks.top();
			delete top;
			blocks.pop();
			top = blocks.top();
		}
	}
    
    	void generateCode(NBlock& root);
    	GenericValue runCode();
    	std::map<std::string, Value*>& locals() { return blocks.top()->locals; }
    	std::map<std::string, GType>& localTypes() { return blocks.top()->localTypes; }
    	BasicBlock *currentBlock() { return blocks.top()->block; }
    	void pushBlock(BasicBlock *block) { blocks.push(new CodeGenBlock(blocks.empty()?0:blocks.top())); blocks.top()->block = block; }
    	void popBlock() { CodeGenBlock *top = blocks.top(); blocks.pop(); delete top; }
};
