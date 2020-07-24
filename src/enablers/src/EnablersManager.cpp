/*
 * Copyright 2019 - 2020 Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "EnablersManager.hpp"
#include "Noelle.hpp"
#include "LoopDistribution.hpp"
#include "LoopUnroll.hpp"

using namespace llvm;

EnablersManager::EnablersManager()
  :
  ModulePass{ID}
  {

  return ;
}

bool EnablersManager::runOnModule (Module &M) {

  /*
   * Check if enablers have been enabled.
   */
  if (!this->enableEnablers){
    return false;
  }
  errs() << "EnablersManager: Start\n";

  /*
   * Fetch the outputs of the passes we rely on.
   */
  auto& noelle = getAnalysis<Noelle>();

  /*
   * Create the enablers.
   */
  auto loopDist = LoopDistribution();
  auto loopUnroll = LoopUnroll();
  auto loopWhilify = LoopWhilifier(noelle);
  auto loopInvariantCodeMotion = LoopInvariantCodeMotion(noelle);
  auto scevSimplification = SCEVSimplification(noelle);

  /*
   * Fetch all the loops we want to parallelize.
   */
  auto loopsToParallelize = noelle.getFilteredLoopStructures();
  errs() << "EnablersManager:  Try to improve all " << loopsToParallelize->size() << " loops, one at a time\n";

  /*
   * Parallelize the loops selected.
   */
  auto modified = false;
  std::unordered_map<Function *, bool> modifiedFunctions;
  for (auto i = 0; i < loopsToParallelize->size(); ++i){

    /*
     * Fetch the function that contains the current loop.
     */
    auto loopStructure = (*loopsToParallelize)[i];
    auto header = loopStructure->getHeader();
    auto f = header->getParent();

    /*
     * Check if we have already modified the function.
     */
    if (modifiedFunctions[f]){
      errs() << "EnablersManager:   The current loop belongs to the function " << f->getName() << " , which has already been modified.\n" ;
      continue ;
    }

    /*
     * Compute loop dependence info
     */
    auto ldi = noelle.getFilteredLoopDependenceInfo(loopStructure);

    /*
     * Improve the current loop.
     */
    modifiedFunctions[f] |= this->applyEnablers(
      ldi,
      noelle,
      loopDist,
      loopUnroll,
      loopWhilify,
      loopInvariantCodeMotion,
      scevSimplification
    );
    modified |= modifiedFunctions[f];
  }

  /*
   * Free the memory.
   */
  delete loopsToParallelize;

  errs() << "EnablersManager: Exit\n";
  return modified;
}
