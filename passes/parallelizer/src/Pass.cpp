/*
 * Copyright 2016 - 2019  Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "Parallelizer.hpp"

using namespace llvm;

/*
 * Options of the Parallelizer pass.
 */
static cl::opt<bool> ForceParallelization("dswp-force", cl::ZeroOrMore, cl::Hidden, cl::desc("Force the parallelization"));
static cl::opt<bool> ForceNoSCCPartition("dswp-no-scc-merge", cl::ZeroOrMore, cl::Hidden, cl::desc("Force no SCC merging when parallelizing"));
static cl::opt<int> Verbose("dswp-verbose", cl::ZeroOrMore, cl::Hidden, cl::desc("Verbose output (0: disabled, 1: minimal, 2: maximal)"));
static cl::opt<int> MinimumHotness("noelle-min-hot", cl::ZeroOrMore, cl::Hidden, cl::desc("Minimum hotness of code to be parallelized"));

/*
 * Command line overrides for some autotuned parameters
 */
static cl::opt<int> DOALLChunkSizeOverride("doall-chunk-size", cl::ZeroOrMore, cl::Hidden, cl::desc("DOALL chunk size"));
static cl::opt<int> DOALLCoresPerOverride("doall-cores-per", cl::ZeroOrMore, cl::Hidden, cl::desc("DOALL number of cores"));
static cl::opt<int> DSWPCoresPerOverride("dswp-cores-per", cl::ZeroOrMore, cl::Hidden, cl::desc("DSWP number of cores"));

Parallelizer::Parallelizer()
  :
  ModulePass{ID}, 
  forceParallelization{false},
  forceNoSCCPartition{false},
  verbose{Verbosity::Disabled},
  minHot{0}
  {

  return ;
}

bool Parallelizer::doInitialization (Module &M) {
  this->verbose = static_cast<Verbosity>(Verbose.getValue());
  this->minHot = ((double)(MinimumHotness.getValue())) / 100;
  this->forceParallelization |= (ForceParallelization.getNumOccurrences() > 0);
  this->forceNoSCCPartition |= (ForceNoSCCPartition.getNumOccurrences() > 0);

  return false; 
}

bool Parallelizer::runOnModule (Module &M) {

  /*
   * Fetch the outputs of the passes we rely on.
   */
  auto& parallelizationFramework = getAnalysis<Parallelization>();
  auto heuristics = getAnalysis<HeuristicsPass>().getHeuristics();

  /*
   * Allocate the parallelization techniques.
   */
  DSWP dswp{
    M,
    this->forceParallelization,
    !this->forceNoSCCPartition,
    this->verbose,
    DSWPCoresPerOverride.getValue(),
  };
  DOALL doall{
    M,
    this->verbose,
    DOALLCoresPerOverride.getValue(),
    DOALLChunkSizeOverride.getValue()
  };
  HELIX helix{M, this->verbose};

  /*
   * Collect information about C++ code we link parallelized loops with.
   */
  errs() << "Parallelizer: Analyzing the module " << M.getName() << "\n";
  if (!collectThreadPoolHelperFunctionsAndTypes(M, parallelizationFramework)) {
    errs() << "Parallelizer utils not included!\n";
    return false;
  }

  /*
   * Fetch all the loops we want to parallelize.
   */
  auto loopsToParallelize = parallelizationFramework.getModuleLoops(&M, this->minHot);
  errs() << "Parallelizer:  There are " << loopsToParallelize->size() << " loops to parallelize\n";
  for (auto loop : *loopsToParallelize){
    errs() << "Parallelizer:    Function \"" << loop->function->getName() << "\"\n";
    errs() << "Parallelizer:    Try to parallelize the loop \"" << *loop->header->getFirstNonPHI() << "\"\n";
  }

  /*
   * Parallelize the loops selected.
   */
  errs() << "Parallelizer:  Parallelize all " << loopsToParallelize->size() << " loops, one at a time\n";
  auto modified = false;
  for (auto loop : *loopsToParallelize){

    /*
     * Parallelize the current loop with Parallelizer.
     */
    modified |= this->parallelizeLoop(loop, parallelizationFramework, dswp, doall, helix, heuristics);

    /*
     * Free the memory.
     */
    delete loop;
  }

  delete loopsToParallelize;

  return modified;
}

void Parallelizer::getAnalysisUsage (AnalysisUsage &AU) const {
  AU.addRequired<PDGAnalysis>();
  AU.addRequired<Parallelization>();
  AU.addRequired<HeuristicsPass>();
  AU.addRequired<ScalarEvolutionWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();

  return ;
}

// Next there is code to register your pass to "opt"
char llvm::Parallelizer::ID = 0;
static RegisterPass<Parallelizer> X("parallelizer", "Automatic parallelization of sequential code");

// Next there is code to register your pass to "clang"
static Parallelizer * _PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new Parallelizer());}}); // ** for -Ox
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new Parallelizer());}});// ** for -O0
