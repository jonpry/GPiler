#include "node.h"
#include "codegen.h"
#include "parser.hpp"

#include "llvm/LLVMContext.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/SourceMgr.h"

void compile(Module &mod){
 	mod.setTargetTriple(Triple::normalize("ptx64-unknown-unknown"));
	Triple TheTriple(mod.getTargetTriple());
  	if (TheTriple.getTriple().empty())
		cout << "Could not locate tripple\n";
	else
		cout << "Triple: " << TheTriple.getTriple() << "\n";

	std::string march = "ptx64";
	const Target *TheTarget = 0;
    	for (TargetRegistry::iterator it = TargetRegistry::begin(); it != TargetRegistry::end(); ++it) {
		cout << it->getName() << "\n";
      		if (march == it->getName()) {
        		TheTarget = &*it;
        		break;
		}
      	}
    

 	if (!TheTarget) {
      		cout << ": error: invalid target '" << march << "'.\n";

    	}
/*
    // Adjust the triple to match (if known), otherwise stick with the
    // module/host triple.
    Triple::ArchType Type = Triple::getArchTypeForLLVMName(MArch);
    if (Type != Triple::UnknownArch)
      TheTriple.setArch(Type); */
}
