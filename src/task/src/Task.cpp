/*
 * Copyright 2016 - 2019  Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "Task.hpp"

using namespace llvm;

Task::Task(){
  return ;
}
      
bool Task::isAnOriginalLiveIn (Value *v) const {
  if (this->liveInClones.find(v) == this->liveInClones.end()){
    return false;
  }

  return true;
}

Value * Task::getCloneOfOriginalLiveIn (Value *o) const {
  if (!this->isAnOriginalLiveIn(o)){
    return nullptr;
  }

  return this->liveInClones.at(o);
}

void Task::addLiveIn (Value *original, Value *internal) {
  this->liveInClones[original] = internal;

  return ;
}

std::unordered_set<Value *> Task::getOriginalLiveIns (void) const {
  std::unordered_set<Value *> s;
  for (auto p : this->liveInClones){
    s.insert(p.first);
  }

  return s;
}
