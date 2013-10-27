/* 
GPiler - compile.cpp
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

#include <cstdio>

#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#define FOR_NV

#ifdef FOR_NV
	#define TRIPLE "nvptx64-unknown-unknown"
	#define MARCH "nvptx64"
#else
#if 0
	#define TRIPLE "x86_64-unknown-linux-gnu"
	#define MARCH "x86-64"
#else
	#define TRIPLE "cpp"
	#define MARCH "cpp"
#endif
#endif

void AddOptimizationPasses(PassManagerBase &MPM, FunctionPassManager &FPM, unsigned OptLevel, unsigned SizeLevel) {
    PassManagerBuilder Builder;
    FPM.add(createVerifierPass());
    Builder.OptLevel = OptLevel;
    Builder.SizeLevel = SizeLevel;

    Builder.Inliner = createAlwaysInlinerPass();
    Builder.DisableUnrollLoops = OptLevel == 0;
    Builder.populateFunctionPassManager(FPM);
    Builder.populateModulePassManager(MPM);

}

static void AddStandardCompilePasses(PassManagerBase &PM) {
    PM.add(createVerifierPass()); // Verify that input is correc
    PassManagerBuilder Builder;
    Builder.Inliner = createFunctionInliningPass();
    Builder.OptLevel = 3;
    Builder.populateModulePassManager(PM);
}

void compile(Module &mod){
	InitializeAllTargets();
  	InitializeAllTargetMCs();
  	InitializeAllAsmPrinters();
  	InitializeAllAsmParsers();

  	// Initialize codegen and IR passes used by llc so that the -print-after,
  	// -print-before, and -stop-after options work.
  	PassRegistry *Registry = PassRegistry::getPassRegistry();
  	initializeCore(*Registry);
  	initializeCodeGen(*Registry);
  	initializeLoopStrengthReducePass(*Registry);
  	initializeLowerIntrinsicsPass(*Registry);
  	initializeUnreachableBlockElimPass(*Registry);
    	initializeScalarOpts(*Registry);
    	initializeIPO(*Registry);
    	initializeAnalysis(*Registry);
    	initializeIPA(*Registry);
    	initializeTransformUtils(*Registry);
    	initializeInstCombine(*Registry);
    	initializeInstrumentation(*Registry);
    	initializeTarget(*Registry);


 	mod.setTargetTriple(Triple::normalize(TRIPLE));
	Triple TheTriple(mod.getTargetTriple());
  	if (TheTriple.getTriple().empty())
		cout << "Could not locate triple\n";
	else
		cout << "Triple: " << TheTriple.getTriple() << "\n";

	std::string march = MARCH;
	const Target *TheTarget = 0;
    	for (TargetRegistry::iterator it = TargetRegistry::begin(); it != TargetRegistry::end(); ++it) {
	//	cout << it->getName() << "\n";
      		if (march == it->getName()) {
        		TheTarget = &*it;
        		break;
		}
      	}
    

 	if (!TheTarget) {
      		cout << ": error: invalid target '" << march << "'.\n";

    	}

    	// Adjust the triple to match (if known), otherwise stick with the
    	// module/host triple.
    	Triple::ArchType Type = Triple::getArchTypeForLLVMName(march);
    	if (Type != Triple::UnknownArch)
      		TheTriple.setArch(Type); 

	std::string mcpu = "sm_20";
	std::string FeaturesStr = "";


  	TargetOptions Options;
  	Options.LessPreciseFPMADOption = false;
  	Options.NoFramePointerElim = false;
  //	Options.AllowFPOpFusion = true;
  	Options.UnsafeFPMath = false;
  	Options.NoInfsFPMath = false;
  	Options.NoNaNsFPMath = false;
  	Options.HonorSignDependentRoundingFPMathOption = false;
  	Options.UseSoftFloat = false;
  	Options.NoZerosInBSS = false;
  	Options.GuaranteedTailCallOpt = false;
  	Options.DisableTailCalls = false;
  	Options.StackAlignmentOverride = 0;
  	Options.TrapFuncName = "";
  	Options.PositionIndependentExecutable = false;
  	Options.EnableSegmentedStacks = false;
	Options.UseInitArray = false;

	std::auto_ptr<TargetMachine>
    		target(TheTarget->createTargetMachine(TheTriple.getTriple(),
                                          mcpu, FeaturesStr,
                                          Options, Reloc::Default, CodeModel::Default,
						CodeGenOpt::Aggressive));

 	assert(target.get() && "Could not allocate target machine!");
  	TargetMachine &Target = *target.get();

	// Build up all of the passes that we want to do to the module.
  	PassManager PM;
	FunctionPassManager FPM(&mod);
	
	// Add intenal analysis passes from the target machine.
  	Target.addAnalysisPasses(PM);

  	// Add the target data from the target machine, if it exists, or the module.
 	if (const DataLayout *TD = Target.getDataLayout()){
    		PM.add(new DataLayout(*TD));
		FPM.add(new DataLayout(*TD));
  	}else
    		PM.add(new DataLayout(&mod));

	AddStandardCompilePasses(PM);
	PM.add( createFunctionInliningPass(275));

   	FPM.doInitialization();
    	for (Module::iterator F = mod.begin(), E = mod.end(); F != E; ++F)
        	FPM.run(*F);
    	FPM.doFinalization();


    	AddOptimizationPasses(PM, FPM, 3, 0);
    	PM.add(createVerifierPass());



	// Override default to generate verbose assembly.
  	Target.setAsmVerbosityDefault(true);

  	{
		TargetMachine::CodeGenFileType FileType = TargetMachine::CGFT_AssemblyFile;

    		// Ask the target to add backend passes as necessary.
		std::string error;
 		tool_output_file *Out = new tool_output_file("out.ptx", error);
		formatted_raw_ostream FOS(Out->os());
    		if (Target.addPassesToEmitFile(PM, FOS, FileType, false)) {
      				cout << ": target does not support generation of this file type!\n";
    		}

    		PM.run(mod);
  	}
}
