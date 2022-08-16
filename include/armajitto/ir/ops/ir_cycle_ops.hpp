#pragma once

#include "armajitto/ir/defs/arg_refs.hpp"
#include "armajitto/ir/defs/memory_access.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Add single bus memory access cycles
//   cycles.s [s/n][c/d][b/h/w]:<var/imm:address>
//     s/n = {S}equential / {N}onsequential
//     c/d = {C}ode / {D}ata
//     b/h/w = {B}yte / {H}alf / {W}ord
//
// Computes the number of cycles for the specified memory access and adds it to the current cycle count.
// This assumes only a single bus is available for memory accesses, as seen in ARMv4T CPUs such as ARM7TDMI.
struct IRAddSingleBusMemCyclesOp : public IROpBase<IROpcodeType::AddSingleBusMemCycles> {
    MemAccessType type;
    MemAccessBus bus;
    MemAccessSize size;
    VarOrImmArg address;
};

// Add multiplication internal cycles
//   cycles.[u/s]m <var/imm:multiplier>
//     u/s = {U}nsigned / {S}igned
//
// Computes the number of cycles for the specified multiplication operation and adds it to the current cycle count.
struct IRAddMulCyclesOp : public IROpBase<IROpcodeType::AddMulCycles> {
    bool sign;
    VarOrImmArg address;
};

// Parallel code/data bus cycle counting (e.g. ARM946E-S)
// Add dual bus memory access cycles.
//    cycles.d <code cycles>, <data cycles>    (default case; accesses may be parallel or sequential)
//    cycles.d <code cycles> | <data cycles>   (when accesses are known to be parallel -- max(code, data))
//                                             (emitted by optimizer only)
//    cycles.d <code cycles> + <data cycles>   (when accesses are known to be sequential -- code + data)
//                                             (emitted by optimizer only)
//  <code/data cycles> specifies one of:
//    [s/n][c/d][b/h/w]:<var/imm:address>   (memory accesses; C/D matches <code/data cycles>)
//    f:<var/imm:count>                     (fixed cycle count; internal cycles or known/optimized memory cycle counts)
//  where:
//    s/n = {S}equential / {N}onsequential
//    c/d = {C}ode / {D}ata
//    b/h/w = {B}yte / {H}alf / {W}ord
//    f = {F}ixed
//  Constraints:
//    {C}ode accesses can only be {H}alf or {W}ord
//    <code cycle> is either {C}ode or {F}ixed
//    <data cycle> is either {D}ata or {F}ixed
//
struct IRAddDualBusCyclesOp : public IROpBase<IROpcodeType::AddDualBusCycles> {
    struct Params {
        bool fixed;
        MemAccessType type;         // when fixed == false
        MemAccessSize size;         // when fixed == false
        VarOrImmArg addressOrCount; // address when fixed == false, count when fixed == true
    };
    enum class Parallelism { Unknown, Sequential, Parallel };

    Params code;
    Params data;
    Parallelism parallelism;
};

} // namespace armajitto::ir
