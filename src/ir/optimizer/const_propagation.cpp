#include "const_propagation.hpp"

namespace armajitto::ir {

void ConstPropagationOptimizerPass::Optimize(Emitter &emitter) {
    // TODO: emitter needs improvements
    // - insert anywhere
    // - modify, replace or remove any existing instruction
    //
    // Possible API design:
    //   Manage cursor
    //     size_t GetCursorPos()
    //     void SetCursorPos(size_t index)
    //       index is clamped to size
    //       index == size means insert at the end
    //     size_t GetCodeSize()
    //   Get writable pointer to instruction
    //     IROp *GetOp(size_t index)   index >= size returns nullptr
    //     IROp *GetCurrentOp()
    //       shorthand for emitter.GetOp(emitter.GetCursorPos())
    //   Overwrite one instruction
    //     Emitter &Overwrite()   next emitted instruction will overwrite the current instruction
    //       For complex instruction sequences, only the first instruction will overwrite; the rest of the sequence is
    //       appended after that, which is the expected behavior
    //   Erase instructions
    //     void Erase(size_t pos, size_t count)
    //       Erasing beyond the end of the vector is a no-op
    //       (i.e. pos and pos+count are clamped to size-1)
    //
    // Usage examples:
    //   // Begin inserting at specified position
    //   emitter.SetCursorPos(3);
    //   emitter.SetRegister(...);  // inserted at 3
    //   emitter.Add(...);          // inserted at 4
    //   emitter.Subtract(...);     // ... and so on
    //
    //   emitter.Subtract(...);                 // insert
    //   emitter.Overwrite().Compare(...);      // overwrite
    //   emitter.BitClear(...);                 // insert
    //   emitter.Overwrite().UpdateFlags(...);  // overwrite first instruction, insert rest
}

} // namespace armajitto::ir
