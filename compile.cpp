#include "node.h"
#include "codegen.h"
#include "parser.hpp"

#include <cstdio>

#include "llvm/LLVMContext.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Target/TargetData.h"

void compile(Module &mod){
	InitializeAllTargets();
  	InitializeAllTargetMCs();
  	InitializeAllAsmPrinters();
  	InitializeAllAsmParsers();


 	mod.setTargetTriple(Triple::normalize("ptx64-unknown-unknown"));
	Triple TheTriple(mod.getTargetTriple());
  	if (TheTriple.getTriple().empty())
		cout << "Could not locate tripple\n";
	else
		cout << "Triple: " << TheTriple.getTriple() << "\n";

	std::string march = "ptx64";
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
  	Options.PrintMachineCode = false;
  	Options.NoFramePointerElim = false;
  	Options.NoFramePointerElimNonLeaf = false;
  	Options.NoExcessFPPrecision = false;
  	Options.UnsafeFPMath = false;
  	Options.NoInfsFPMath = false;
  	Options.NoNaNsFPMath = false;
  	Options.HonorSignDependentRoundingFPMathOption = false;
  	Options.UseSoftFloat = false;
  	Options.NoZerosInBSS = false;
  	Options.GuaranteedTailCallOpt = false;
  	Options.DisableTailCalls = false;
  	Options.StackAlignmentOverride = 0;
  	Options.RealignStack = true;
  	Options.DisableJumpTables = false;
  	Options.TrapFuncName = "";
  	Options.PositionIndependentExecutable = false;
  	Options.EnableSegmentedStacks = false;

	std::auto_ptr<TargetMachine>
    		target(TheTarget->createTargetMachine(TheTriple.getTriple(),
                                          mcpu, FeaturesStr,
                                          Options));

 	assert(target.get() && "Could not allocate target machine!");
  	TargetMachine &Target = *target.get();

	// Build up all of the passes that we want to do to the module.
  	PassManager PM;
	

  	// Add the target data from the target machine, if it exists, or the module.
  	if (const TargetData *TD = Target.getTargetData())
    		PM.add(new TargetData(*TD));
  	else
    		PM.add(new TargetData(&mod));

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
