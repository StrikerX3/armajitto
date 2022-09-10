#pragma once

#include "ir/defs/arguments.hpp"
#include "ir/ir_ops.hpp"

#include <optional>

namespace armajitto::ir {

// Helper class to track host flag states.
// Add an instance of this class to an optimizer, then add this to the IROp postprocessor stage:
//   m_hostFlagsStateTracker.Update(op);
// Use the flag-named methods to check the state of those specific host flags.
//
// Does not work on a backward scan.
class HostFlagStateTracker {
public:
    void Reset();

    // Update the state of the host flags from the specified IROp.
    void Update(IROp *op);

    // Returns the known flag states.
    arm::Flags Known() const {
        return m_known;
    }

    // Returns the states for known flags.
    arm::Flags State() const {
        return m_state;
    }

    std::optional<bool> Negative() const {
        return Test(arm::Flags::N);
    }

    std::optional<bool> Zero() const {
        return Test(arm::Flags::Z);
    }

    std::optional<bool> Carry() const {
        return Test(arm::Flags::C);
    }

    std::optional<bool> Overflow() const {
        return Test(arm::Flags::V);
    }

private:
    arm::Flags m_known = arm::Flags::None; // known flag states
    arm::Flags m_state = arm::Flags::None; // current state of known flags; unknown flags are always unset

    std::optional<bool> Test(arm::Flags flag) const {
        if (BitmaskEnum(m_known).AllOf(flag)) {
            return BitmaskEnum(m_state).AllOf(flag);
        } else {
            return std::nullopt;
        }
    }

    void Unknown(arm::Flags flags);
    void Known(arm::Flags flags, arm::Flags values);

    // Catch-all method for unused ops, required by the visitor.
    template <typename T>
    void UpdateImpl(T *op) {}

    void UpdateImpl(IRLogicalShiftLeftOp *op);
    void UpdateImpl(IRLogicalShiftRightOp *op);
    void UpdateImpl(IRArithmeticShiftRightOp *op);
    void UpdateImpl(IRRotateRightOp *op);
    void UpdateImpl(IRRotateRightExtendedOp *op);
    void UpdateImpl(IRBitwiseAndOp *op);
    void UpdateImpl(IRBitwiseOrOp *op);
    void UpdateImpl(IRBitwiseXorOp *op);
    void UpdateImpl(IRBitClearOp *op);
    void UpdateImpl(IRAddOp *op);
    void UpdateImpl(IRAddCarryOp *op);
    void UpdateImpl(IRSubtractOp *op);
    void UpdateImpl(IRSubtractCarryOp *op);
    void UpdateImpl(IRMoveOp *op);
    void UpdateImpl(IRMoveNegatedOp *op);
    void UpdateImpl(IRSaturatingAddOp *op);
    void UpdateImpl(IRSaturatingSubtractOp *op);
    void UpdateImpl(IRMultiplyOp *op);
    void UpdateImpl(IRMultiplyLongOp *op);
    void UpdateImpl(IRAddLongOp *op);
    void UpdateImpl(IRStoreFlagsOp *op);
};

} // namespace armajitto::ir
