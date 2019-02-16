/*
 * Copyright 2016 - 2019  Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "HELIX.hpp"
#include "HELIXTask.hpp"
#include "Architecture.hpp"

using namespace llvm ;

void HELIX::addSynchronizations (
  LoopDependenceInfo *LDI,
  std::vector<SequentialSegment *> *sss
  ){
  assert(this->tasks.size() == 1);
  auto helixTask = static_cast<HELIXTask *>(this->tasks[0]);
  IRBuilder<> entryBuilder(helixTask->entryBlock->getTerminator());

  /*
   * Iterate over sequential segments.
   */
  for (auto ss : *sss){

    /*
     * We must execute exactly one wait instruction for each sequential segment, for each loop iteration, and for each thread.
     *
     * Create a new variable at the beginning of the iteration.
     * We call this new variable, ssState.
     * This new variable is reponsible to store the information about whether a wait instruction of the current sequential segment has already been executed in the current iteration for the current thread.
     */
    auto &cxt = LDI->function->getContext();
    auto int64 = IntegerType::get(cxt, 64);
    Value *ssState = entryBuilder.CreateAlloca(int64);

    /*
     * Reset the value of ssState at the beginning of the iteration (i.e., loop header)
     */
    IRBuilder<> headerBuilder(LDI->header->getFirstNonPHI());
    headerBuilder.CreateStore(ConstantInt::get(int64, 0), ssState);

    /*
     * Define a helper to fetch the appropriate ss entry in synchronization arrays
     */
    auto fetchEntry = [ss, &entryBuilder, int64](Value *ssArray) -> Value * {

      /*
       * Compute the offset of the sequential segment entry.
       */
      auto ssID = ss->getID();
      auto ssOffset = ssID * CACHE_LINE_SIZE;

      /*
       * Fetch the pointer to the sequential segment entry.
       */
      auto ssArrayAsInt = entryBuilder.CreatePtrToInt(ssArray, int64);
      auto ssEntryAsInt = entryBuilder.CreateAdd(ConstantInt::get(int64, ssOffset), ssArrayAsInt);
      return entryBuilder.CreateIntToPtr(ssEntryAsInt, ssArray->getType());
    };

    /*
     * Fetch the sequential segment entry in the past and future array
     */
    auto ssPastPtr = fetchEntry(helixTask->ssPastArrayArg);
    auto ssFuturePtr = fetchEntry(helixTask->ssFutureArrayArg);

    /*
     * Define the code that inject wait instructions.
     */
    auto injectWait = [&](Instruction *justAfterEntry) -> void {

      /*
       * Separate out the basic block into 2 halves, the second starting with justAfterEntry
       */
      auto beforeEntryBB = justAfterEntry->getParent();
      auto ssEntryBB = BasicBlock::Create(cxt, "", helixTask->F);
      IRBuilder<> ssEntryBuilder(ssEntryBB);
      auto afterEntry = justAfterEntry;
      while (afterEntry) {
        auto currentEntry = afterEntry;
        afterEntry = afterEntry->getNextNode();
        currentEntry->removeFromParent();
        ssEntryBuilder.Insert(currentEntry);
      }

      /*
       * Redirect PHI node incoming blocks from beforeEntryBB to ssEntryBB
       */
      for (auto succToEntry : successors(ssEntryBB)) {
        for (auto &phi : succToEntry->phis()) {
          auto incomingIndex = phi.getBasicBlockIndex(beforeEntryBB);
          phi.setIncomingBlock(incomingIndex, ssEntryBB);
        }
      }

      /*
       * Inject a call to HELIX_wait just before "justAfterEntry"
       * Set the ssState just after the call to HELIX_wait.
       * This will keep track of the fact that we have executed wait for ss in the current iteration.
       */
      auto ssWaitBB = BasicBlock::Create(cxt, "", helixTask->F);
      IRBuilder<> ssWaitBuilder(ssWaitBB);
      auto wait = ssWaitBuilder.CreateCall(this->waitSSCall, { ssPastPtr });
      ssWaitBuilder.CreateStore(ConstantInt::get(int64, 1), ssState);
      ssWaitBuilder.CreateBr(ssEntryBB);

      /*
       * Check if the ssState has been set already.
       * If it did, then we have already executed the wait to enter this ss and must not invoke it again.
       * If it didn't, then we need to invoke HELIX_wait.
       */
      IRBuilder<> beforeEntryBuilder(beforeEntryBB);
      auto ssStateLoad = beforeEntryBuilder.CreateLoad(ssState);
      auto needToWait = beforeEntryBuilder.CreateICmpEQ(ssStateLoad, ConstantInt::get(int64, 0));
      beforeEntryBuilder.CreateCondBr(needToWait, ssWaitBB, ssEntryBB);

      /*
       * Track the call to wait
       */
      helixTask->waits.insert(cast<CallInst>(wait));
    };

    /*
     * Define the code that inject wait instructions.
     */
    auto injectSignal = [&](Instruction *justBeforeExit) -> void {

      /*
       * Inject a call to HELIX_signal just after "justBeforeExit" 
       */
      auto terminator = justBeforeExit->getParent()->getTerminator();
      Instruction *insertPoint = terminator == justBeforeExit ? terminator : justBeforeExit->getNextNode();
      IRBuilder<> beforeExitBuilder(insertPoint);
      auto signal = beforeExitBuilder.CreateCall(this->signalSSCall, { ssFuturePtr });

      /*
       * Track the call to signal
       */
      helixTask->signals.insert(cast<CallInst>(signal));
    };

    /*
     * Inject waits.
     */
    ss->forEachEntry(injectWait);

    /*
     * Inject signals at sequential segment exits
     * Inject signals at loop exits
     */
    ss->forEachExit(injectSignal);
    for (auto exitBB : LDI->loopExitBlocks) {
      injectSignal(exitBB->getTerminator());
    }
  }

  return ;
}
