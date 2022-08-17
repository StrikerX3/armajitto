#pragma once

namespace armajitto::ir {

enum class IROpcodeType {
    // Register access
    GetRegister,
    SetRegister,
    GetCPSR,
    SetCPSR,
    GetSPSR,
    SetSPSR,

    // Memory access
    MemRead,
    MemWrite,

    // ALU operations
    LogicalShiftLeft,
    LogicalShiftRight,
    ArithmeticShiftRight,
    RotateRight,
    RotateRightExtend,

    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    BitClear,
    CountLeadingZeros,

    Add,
    AddCarry,
    Subtract,
    SubtractCarry,

    Move,
    MoveNegated,

    SaturatingAdd,
    SaturatingSubtract,

    Multiply,
    MultiplyLong,
    AddLong,

    // Flag manipulation
    StoreFlags,
    UpdateFlags,
    UpdateStickyOverflow,

    // Branching
    Branch,
    BranchExchange,

    // Coprocessor operations
    LoadCopRegister,
    StoreCopRegister,
    // TODO: CDP, CDP2
    // TODO: LDC/STC, LDC2/STC2
    // TODO: MCRR/MRRC

    // Miscellaneous operations
    Constant,
};

} // namespace armajitto::ir
