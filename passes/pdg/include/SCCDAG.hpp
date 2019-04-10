/*
 * Copyright 2016 - 2019  Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#pragma once

#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/ADT/iterator_range.h"
#include <set>
#include <unordered_map>

//#include "liberty/Utilities/BitMatrix.h"
#include "BitMatrix.h"

#include "DGBase.hpp"
#include "SCC.hpp"
#include "PDG.hpp"

using namespace std;
using namespace llvm;

namespace llvm {

  /*
   * Program Dependence Graph.
   */
  class SCCDAG : public DG<SCC> {
    public:
      typedef std::vector<SCC *> SCCSet;

      static SCCDAG * createSCCDAGFrom (PDG *);

      SCCDAG() ;

      //SIMONE: it would be fantastic to have a method like "getAllSCCsWithNoInternalIncomingEdges"

      void mergeSCCs(std::set<DGNode<SCC> *> &sccSet);

      SCC *sccOfValue (Value *val) const;
      ~SCCDAG() ;

      void computeReachabilityAmongSCCs();

      bool orderedBefore(const SCC *earlySCC, const SCCSet &lates) const;
      bool orderedBefore(const SCCSet &earlies, const SCC *lateSCC) const;
      bool orderedBefore(const SCC *earlySCC, const SCC *lateSCC) const;

      unsigned getSCCIndex(const SCC *scc) const {
        auto sccF = sccIndexes.find(scc);
        return sccF->second;
      };

    protected:
      void markValuesInSCC();
      void markEdgesAndSubEdges();

      unordered_map<Value *, DGNode<SCC> *> valueToSCCNode;

    private:
      BitMatrix ordered;
      bool ordered_dirty;
      std::unordered_map<const SCC*, unsigned> sccIndexes;

      typedef std::unordered_map<DGNode<Value> *, int> DGNode2Index;

      static void visit(DGNode<Value> *pdgNode, PDG *pdg, unsigned &index,
                        DGNode2Index &idx, DGNode2Index &low,
                        std::vector<DGNode<Value> *> &stack, SCCDAG *sccdag);
  };
}
