#pragma once

namespace armajitto::ir {

enum class IROpcodeType {
    // Register access
    LoadGPR,
    StoreGPR,
    LoadCPSR,
    StoreCPSR,
    LoadSPSR,
    StoreSPSR,

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
    BitwiseXor,
    Subtract,
    ReverseSubtract,
    Add,
    AddCarry,
    SubtractCarry,
    ReverseSubtractCarry,
    BitwiseOr,
    Move,
    BitClear,
    MoveNegated,
    CountLeadingZeros,
    SaturatingAdd,
    SaturatingSubtract,
    Multiply,
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
};

} // namespace armajitto::ir
