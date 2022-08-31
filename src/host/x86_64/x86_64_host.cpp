#include "armajitto/host/x86_64/x86_64_host.hpp"

#include "armajitto/host/x86_64/cpuid.hpp"
#include "armajitto/ir/ops/ir_ops_visitor.hpp"
#include "armajitto/util/pointer_cast.hpp"

#include "abi.hpp"
#include "vtune.hpp"
#include "x86_64_compiler.hpp"
#include "x86_64_flags.hpp"

#include <limits>

namespace armajitto::x86_64 {

x64Host::x64Host(Context &context)
    : Host(context)
    , m_codegen(Xbyak::DEFAULT_MAX_CODE_SIZE, nullptr, &m_alloc) {

    CompileProlog();
    CompileEpilog();
}

HostCode x64Host::Compile(ir::BasicBlock &block) {
    const auto blockLocKey = block.Location().ToUint64();
    auto &cachedBlock = m_blockCache[blockLocKey];
    Compiler compiler{m_context, m_codegen};
    auto &codegen = compiler.codegen;

    compiler.Analyze(block);

    auto fnPtr = codegen.getCurr<HostCode>();
    codegen.setProtectModeRW();

    Xbyak::Label lblCondFail{};
    Xbyak::Label lblCondPass{};

    compiler.CompileCondCheck(block.Condition(), lblCondFail);

    // Compile block code
    auto *op = block.Head();
    while (op != nullptr) {
        compiler.PreProcessOp(op);
        ir::VisitIROp(op, [&compiler](const auto *op) -> void { compiler.CompileOp(op); });
        compiler.PostProcessOp(op);
        op = op->Next();
    }

    // Skip over condition fail block
    codegen.jmp(lblCondPass);

    // Update PC if condition fails
    codegen.L(lblCondFail);
    const auto pcRegOffset = m_context.GetARMState().GPROffset(arm::GPR::PC, compiler.mode);
    const auto instrSize = block.Location().IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t);
    codegen.mov(dword[abi::kARMStateReg + pcRegOffset], block.Location().PC() + block.InstructionCount() * instrSize);
    // TODO: increment cycles for failing the check

    codegen.L(lblCondPass);

    // TODO: cycle counting
    // TODO: check cycles and IRQ

    // Go to next block or epilog
    using Terminal = ir::BasicBlock::Terminal;
    switch (block.GetTerminal()) {
    case Terminal::BranchToKnownAddress:
    case Terminal::ContinueExecution: {
        auto targetLoc = block.GetTerminalLocation();
        auto code = GetCodeForLocation(targetLoc);
        if (code != 0) {
            // Go to the compiled code's address directly
            /*m_codegen.mov(rcx, code);
            m_codegen.jmp(rcx);*/
            m_codegen.jmp((void *)code, Xbyak::CodeGenerator::T_NEAR);
            compiler.regAlloc.ReleaseTemporaries();
        } else {
            // Store this code location to be patched later
            m_patches[targetLoc.ToUint64()].push_back({blockLocKey, codegen.getCurr()});

            // Go to epilog if there is no compiled code at the target address
            /*codegen.mov(abi::kNonvolatileRegs[0], m_epilog);
            codegen.jmp(abi::kNonvolatileRegs[0]);*/
            m_codegen.jmp((void *)m_epilog, Xbyak::CodeGenerator::T_NEAR);
        }
        break;
    }
    case Terminal::Return:
        // Go to epilog
        /*codegen.mov(abi::kNonvolatileRegs[0], m_epilog);
        codegen.jmp(abi::kNonvolatileRegs[0]);*/
        m_codegen.jmp((void *)m_epilog, Xbyak::CodeGenerator::T_NEAR);
        break;
    }

    codegen.setProtectModeRE();

    cachedBlock.code = fnPtr;
    vtune::ReportBasicBlock(fnPtr, codegen.getCurr<uintptr_t>(), block.Location());

    // Patch references to this block
    auto itPatches = m_patches.find(block.Location().ToUint64());
    if (itPatches != m_patches.end()) {
        for (PatchInfo &patchInfo : itPatches->second) {
            auto itPatchBlock = m_blockCache.find(patchInfo.cachedBlockKey);
            if (itPatchBlock != m_blockCache.end()) {
                // Edit code
                m_codegen.setProtectModeRW();

                // Go to patch location
                auto prevSize = m_codegen.getSize();
                m_codegen.setSize(patchInfo.codePos - m_codegen.getCode());

                // Overwrite a jump to the compiled code's address directly

                // TODO: different patch types
                /*m_codegen.mov(rcx, fnPtr);
                m_codegen.jmp(rcx);*/

                // If target is close enough, emit up to three NOPs, otherwise emit a JMP
                auto distToTarget = (const uint8_t *)fnPtr - patchInfo.codePos;
                if (distToTarget >= 1 && distToTarget <= 27) {
                    for (;;) {
                        if (distToTarget > 9) {
                            m_codegen.nop(9);
                            distToTarget -= 9;
                        } else {
                            m_codegen.nop(distToTarget);
                            break;
                        }
                    }
                } else {
                    m_codegen.jmp((void *)fnPtr, Xbyak::CodeGenerator::T_NEAR);
                }

                // Restore code generator position
                m_codegen.setSize(prevSize);
                m_codegen.setProtectModeRE();
            }
        }
        m_patches.erase(itPatches);
    }

    return fnPtr;
}

void x64Host::CompileProlog() {
    auto &armState = m_context.GetARMState();

    m_prolog = m_codegen.getCurr<PrologFn>();
    m_codegen.setProtectModeRW();

    // Push all nonvolatile registers
    for (auto &reg : abi::kNonvolatileRegs) {
        m_codegen.push(reg);
    }

    // Calculate current stack size
    uint64_t stackSize = abi::kNonvolatileRegs.size() * sizeof(uint64_t);
    stackSize += sizeof(uint64_t); // +1 for RIP pushed by call

    // Calculate offset needed to compensate for stack misalignment
    m_stackAlignmentOffset = abi::Align<abi::kStackAlignmentShift>(stackSize) - stackSize;

    // Setup stack -- make space for register spill area
    // Also include the stack alignment offset
    m_codegen.sub(rsp, abi::kStackReserveSize + m_stackAlignmentOffset);

    // Copy CPSR NZCV flags to ah/al
    auto flagsReg = abi::kHostFlagsReg;
    m_codegen.mov(flagsReg, dword[CastUintPtr(&armState.CPSR())]);
    m_codegen.shr(flagsReg, ARMflgNZCVShift); // Shift NZCV bits to [3..0]
    if (CPUID::HasFastPDEPAndPEXT()) {
        // AH       AL
        // SZ0A0P1C -------V
        // NZ.....C .......V
        auto depMask = abi::kNonvolatileRegs[0];
        m_codegen.mov(depMask.cvt32(), 0b11000001'00000001u); // Deposit bit mask: NZ-----C -------V
        m_codegen.pdep(flagsReg, flagsReg, depMask.cvt32());
    } else {
        m_codegen.imul(flagsReg, flagsReg, ARMTox64FlagsMult); // -------- -------- NZCV-NZC V---NZCV
        m_codegen.and_(flagsReg, x64FlagsMask);                // -------- -------- NZ-----C -------V
    }

    // Setup static registers and call block function
    auto funcAddr = abi::kNonvolatileRegs.back();
    m_codegen.mov(funcAddr, abi::kIntArgRegs[0]);             // Get block code pointer from 1st arg
    m_codegen.mov(abi::kARMStateReg, CastUintPtr(&armState)); // Set ARM state pointer
    m_codegen.jmp(funcAddr);                                  // Jump to block code

    m_codegen.setProtectModeRE();
    vtune::ReportCode(CastUintPtr(m_prolog), m_codegen.getCurr<uintptr_t>(), "__prolog");
}

void x64Host::CompileEpilog() {
    m_epilog = m_codegen.getCurr<HostCode>();
    m_codegen.setProtectModeRW();

    // Cleanup stack
    m_codegen.add(rsp, abi::kStackReserveSize + m_stackAlignmentOffset);

    // Pop all nonvolatile registers
    for (auto it = abi::kNonvolatileRegs.rbegin(); it != abi::kNonvolatileRegs.rend(); it++) {
        m_codegen.pop(*it);
    }

    // Return from call
    m_codegen.ret();

    m_codegen.setProtectModeRE();
    vtune::ReportCode(m_epilog, m_codegen.getCurr<uintptr_t>(), "__epilog");
}

} // namespace armajitto::x86_64
