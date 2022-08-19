#include "const_propagation.hpp"

namespace armajitto::ir {

void ConstPropagationOptimizerPass::Optimize(Emitter &emitter) {
    // TODO: emitter needs improvements
    // - insert anywhere
    // - modify, replace or remove any existing instruction
    //
    // Possible API design:
    //   Manage cursor
    //     size_t GetCodeSize()
    //     size_t GetCursorPos()
    //     void SetCursorPos(size_t index)
    //       index is clamped to size
    //       index == size means insert at the end
    //       (assert that index <= size)
    //   Get writable pointer to instruction
    //     IROp *GetOp(size_t index)
    //       index >= size returns nullptr
    //       (assert that index < size)
    //     IROp *GetCurrentOp()
    //       shorthand for emitter.GetOp(emitter.GetCursorPos())
    //   Overwrite one instruction
    //     Emitter &Overwrite()
    //       Next emitted instruction will overwrite the current instruction.
    //       Returns the emitter itself for method chaining.
    //       For complex instruction sequences, only the first instruction will overwrite. The rest of the sequence is
    //       appended after that, which is the expected behavior.
    //   Erase instructions
    //     void Erase(size_t pos, size_t count = 1)
    //       Erasing beyond the end of the vector is a no-op.
    //       (i.e. pos and pos+count are clamped to size-1)
    //       (though there should be an assert for correctness)
    //     void EraseNext(size_t count = 1)
    //       shorthand for Erase(GetCursorPos(), count)
    //
    // Usage examples:
    //   emitter.SetCursorPos(3);   // move insertion point to index 3 (i.e., between the 3rd and 4th elements)
    //   emitter.SetRegister(...);  // inserted at 3
    //   emitter.Add(...);          // inserted at 4
    //   emitter.Subtract(...);     // ... and so on
    //
    //   auto *op = emitter.GetCurrentOp();  // get op at the current cursor position
    //   op-> ... ; // do something with it
    //
    //   emitter.Subtract(...);                 // insert
    //   emitter.Overwrite().Compare(...);      // overwrite
    //   emitter.BitClear(...);                 // insert
    //   emitter.Overwrite().UpdateFlags(...);  // overwrite first instruction, insert rest
    // 
    //   emitter.Erase
    //
    //   for (size_t i = 0; i < emitter.GetCodeSize(); i++) {
    //       auto *op = emitter.GetOp(i);
    //       op-> ... ; // manipulate op
    //       // adding/removing instructions in this loop is not recommended
    //   }
}

} // namespace armajitto::ir
