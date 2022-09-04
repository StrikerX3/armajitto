#pragma once

#include "dead_store_elimination_base.hpp"

#include <array>
#include <memory_resource>
#include <vector>

namespace armajitto::ir {

// Performs dead store elimination for GPRs and PSRs.
//
// This algorithm tracks GPR and PSR changes by tagging variables with the GPR or PSR "version" and incrementing it on
// every change. When the GPR or PSR is loaded into a variable, it is tagged with the current version. Operations that
// take a tagged variable, modify the value, and return a new variable tag the output variable with a new version. When
// a tagged variable is stored into GPR or PSR, its version is updated to that of the variable.
//
// Once the algorithm detects an attempt to store an unmodified GPR/PSR value (that is, storing a tagged variable with
// the same version as the GPR/PSR), the store is removed. Additionally, every subsequent load from the GPR/PSR will
// create a variable mapping from the output variable of the load PSR or GPR instruction to the variable that contains
// the current version of the GPR/PSR value, eliminating several redundant sequences of loads and stores.
//
// The same algorithm is applied to CPSR, SPSRs and GPRs in every mode, with a separate version for each individual
// instance of the registers.
//
// Assuming the following IR code fragment:
//                                  PSR version
//  #  instruction                  curr   next    tags ($v<x>=<version>) or substitutions ($v<x>->$v<y>)
//  1  ld $v0, cpsr                 1      2       $v0=1
//  2  add $v1, $v0, #0x4           1      3       $v1=2
//  3  st r0_usr, $v1               1      3
//  4  st cpsr, $v0                 1      3
//  5  ld $v2, cpsr                 1      3       $v2->$v0
//  6  bic $v3, $v2, #0xc0000000    1      4       $v3=3  (note the global increment)
//  7  st cpsr, $v3                 3      4
//  8  ld $v4, r5                   3      4
//  9  st cpsr, $v4                 4      5
//
// Before executing the algorithm, CPSR is initialized with version 1 and the next version is set to 2.
// These are the actions taken by the algorithm for each instruction:
//   1. $v0 is tagged with CPSR version 1.
//   2. Modifies $v0 and outputs the result to $v1, thus $v1 is tagged with CPSR version 2.
//   3. No variables are output, so nothing is done.
//   4. Stores $v0 back into CPSR. Since the version of the variable matches the current CPSR version, the store is
//      redundant and therefore eliminated.
//   5. Loads CPSR into $v2. Since there already exists a variable tagged with version 1, this load is erased and $v2 is
//      mapped to $v0. All subsequent instances of $v2 are replaced with $v1.
//   6. $v2 is replaced with $v0. BIC consumes $v0 and outputs $v3. The latter is tagged with the next CPSR version: 3.
//      Note that the "next CPSR version" is a global counter and not an increment of the currently tagged version.
//   7. Stores $v3 into CPSR, updating the current CPSR version to 3.
//   8. Loads a value into $v4. This variable is not tagged because it does not come from CPSR.
//   9. Stores $v4 into CPSR, which is untagged. CPSR version is updated to version 4 -- the next global version.
//      Additionally, because this overwrites the CPSR value from instruction 7, that write is erased.
//
// This is the resulting code:
//
//     ld $v0, cpsr
//     add $v1, $v0, #0x4
//     st r0_usr, $v1
//     bic $v3, $v0, #0xc0000000
//     ld $v4, r5
//     st cpsr, $v4
//
// Note that the BIC instruction is now a dead store and should be eliminated by the dead variable store pass.
class DeadRegisterStoreEliminationOptimizerPass final : public DeadStoreEliminationOptimizerPassBase {
public:
    DeadRegisterStoreEliminationOptimizerPass(Emitter &emitter, std::pmr::memory_resource &alloc);

private:
    void Process(IRGetRegisterOp *op) final;
    void Process(IRSetRegisterOp *op) final;
    void Process(IRGetCPSROp *op) final;
    void Process(IRSetCPSROp *op) final;
    void Process(IRGetSPSROp *op) final;
    void Process(IRSetSPSROp *op) final;
    void Process(IRMemReadOp *op) final;
    void Process(IRMemWriteOp *op) final;
    void Process(IRPreloadOp *op) final;
    void Process(IRLogicalShiftLeftOp *op) final;
    void Process(IRLogicalShiftRightOp *op) final;
    void Process(IRArithmeticShiftRightOp *op) final;
    void Process(IRRotateRightOp *op) final;
    void Process(IRRotateRightExtendedOp *op) final;
    void Process(IRBitwiseAndOp *op) final;
    void Process(IRBitwiseOrOp *op) final;
    void Process(IRBitwiseXorOp *op) final;
    void Process(IRBitClearOp *op) final;
    void Process(IRCountLeadingZerosOp *op) final;
    void Process(IRAddOp *op) final;
    void Process(IRAddCarryOp *op) final;
    void Process(IRSubtractOp *op) final;
    void Process(IRSubtractCarryOp *op) final;
    void Process(IRMoveOp *op) final;
    void Process(IRMoveNegatedOp *op) final;
    void Process(IRSaturatingAddOp *op) final;
    void Process(IRSaturatingSubtractOp *op) final;
    void Process(IRMultiplyOp *op) final;
    void Process(IRMultiplyLongOp *op) final;
    void Process(IRAddLongOp *op) final;
    // void Process(IRStoreFlagsOp *op) final;
    void Process(IRLoadFlagsOp *op) final;
    void Process(IRLoadStickyOverflowOp *op) final;
    void Process(IRBranchOp *op) final;
    void Process(IRBranchExchangeOp *op) final;
    // void Process(IRLoadCopRegisterOp *op) final;
    void Process(IRStoreCopRegisterOp *op) final;
    // void Process(IRConstantOp *op) final;
    void Process(IRCopyVarOp *op) final;
    // void Process(IRGetBaseVectorAddressOp *op) final;

    // -------------------------------------------------------------------------
    // GPR/PSR read and write tracking

    struct VarWrite {
        Variable var;
        IROp *writeOp = nullptr;
    };

    // 0=CPSR, 1..6=SPSR by mode
    std::array<uintmax_t, 1 + arm::kNumBankedModes> m_psrVersions;
    std::array<IROp *, 1 + arm::kNumBankedModes> m_psrWrites;

    std::array<uintmax_t, 16 * arm::kNumBankedModes> m_gprVersions;
    std::array<IROp *, 16 * arm::kNumBankedModes> m_gprWrites;

    std::pmr::vector<VarWrite> m_versionToVarMap;
    std::pmr::vector<uintmax_t> m_varToVersionMap;

    uintmax_t m_nextVersion;

    static inline size_t SPSRIndex(arm::Mode mode) {
        return arm::NormalizedIndex(mode) + 1;
    }

    void RecordGPRRead(GPRArg gpr, VariableArg var, IROp *loadOp);
    void RecordGPRWrite(GPRArg gpr, VariableArg src, IROp *op);

    void RecordCPSRRead(VariableArg var, IROp *loadOp);
    void RecordCPSRWrite(VariableArg src, IROp *op);

    void RecordSPSRRead(arm::Mode mode, VariableArg var, IROp *loadOp);
    void RecordSPSRWrite(arm::Mode mode, VariableArg src, IROp *op);

    void RecordPSRRead(size_t index, VariableArg var, IROp *loadOp);
    void RecordPSRWrite(size_t index, VariableArg src, IROp *op);

    bool IsTagged(VariableArg var);
    bool IsTagged(VarOrImmArg var);
    void AssignNewVersion(VariableArg var);
    void CopyVersion(VariableArg dst, VariableArg src);

    void SubstituteVar(VariableArg &var);
    void SubstituteVar(VarOrImmArg &var);

    void ResizeVersionToVarMap(size_t index);
    void ResizeVarToVersionMap(size_t index);
};

} // namespace armajitto::ir
