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
#include "llvm/IR/DataLayout.h"

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


 	mod.setTargetTriple(Triple::normalize("nvptx64-unknown-unknown"));
	Triple TheTriple(mod.getTargetTriple());
  	if (TheTriple.getTriple().empty())
		cout << "Could not locate triple\n";
	else
		cout << "Triple: " << TheTriple.getTriple() << "\n";

	std::string march = "nvptx64";
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
                                          Options));

 	assert(target.get() && "Could not allocate target machine!");
  	TargetMachine &Target = *target.get();

	// Build up all of the passes that we want to do to the module.
  	PassManager PM;
	

  	// Add the target data from the target machine, if it exists, or the module.
 	if (const DataLayout *TD = Target.getDataLayout())
    		PM.add(new DataLayout(*TD));
  	else
    		PM.add(new DataLayout(&mod));



  	// Override default to generate verbose assembly.
  	Target.setAsmVerbosityDefault(true);

  	{
		TargetMachine::CodeGenFileType FileType = TargetMachine::CGFT_AssemblyFile;

    		// Ask the target to add backend passes as necessary.
    		if (Target.addPassesToEmitFile(PM, fouts(), FileType, false)) {
      				cout << ": target does not support generation of this file type!\n";
    		}

    		PM.run(mod);
  	}
}
