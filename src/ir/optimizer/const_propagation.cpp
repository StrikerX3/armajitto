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
    //     bool IsCursorAtEnd()
    //       shorthand for GetCursorPos() == GetCodeSize()
    //     void SetCursorPos(size_t index)
    //       (assert that index <= size)
    //       Absolute cursor adjustment.
    //       index is clamped to size
    //       index == size means insert at the end
    //     size_t MoveCursor(int64_t offset)
    //       (assert that (size_t)(index+offset) <= size; also works if the negative offset is too large)
    //       Relative cursor adjustment.
    //       index is clamped to 0..size
    //   Get writable pointer to instruction
    //     IROp *GetOp(size_t index)
    //       (assert that index < size)
    //       index >= size returns nullptr
    //     IROp *GetCurrentOp()
    //       shorthand for GetOp(GetCursorPos())
    //   Overwrite one instruction
    //     Emitter &Overwrite()
    //       Next emitted instruction will overwrite the current instruction.
    //       Returns the emitter itself for method chaining.
    //       For complex instruction sequences, only the first instruction will overwrite. The rest of the sequence is
    //       appended after that, which is the expected behavior.
    //   Erase instructions
    //     void Erase(size_t pos, size_t count = 1)
    //       (assert that pos < size and pos+count <= size)
    //       (assert that count >= 1)
    //       count = 0 is a no-op.
    //       Erasing beyond the end of the vector is a no-op.
    //       (i.e. pos is clamped to size-1 and pos+count is clamped to size)
    //       Also adjusts cursor position towards the beginning.
    //       (i.e. erasing a range of elements that contains the cursor will move it to the first instruction before the
    //       erased range)
    //     void EraseNext(size_t count = 1)
    //       shorthand for Erase(GetCursorPos(), count)
    //
    // Usage examples:
    //   emitter.SetCursorPos(3);   // move cursor to index 3 (i.e., between the 3rd and 4th elements)
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
    //   emitter.Erase(3, 5);   // erases instructions 3..7
    //   emitter.Erase(1);      // erases instruction 1
    //   emitter.EraseNext();   // erases instruction at cursor
    //   emitter.EraseNext(4);  // erases 4 instructions starting at the cursor
    //
    //   for (size_t i = 0; i < emitter.GetCodeSize(); i++) {
    //       auto *op = emitter.GetOp(i);
    //       op-> ... ; // manipulate op
    //       // adding/removing instructions in this loop is not recommended
    //   }
}

} // namespace armajitto::ir
