#include "x86_64_host.hpp"

#include "armajitto/guest/arm/exceptions.hpp"

#include "ir/ops/ir_ops_visitor.hpp"

#include "util/pointer_cast.hpp"

#include "abi.hpp"
#include "cpuid.hpp"
#include "vtune.hpp"
#include "x86_64_compiler.hpp"
#include "x86_64_flags.hpp"

#include <limits>

namespace armajitto::x86_64 {

inline const auto kCycleCountOperand = qword[abi::kVarSpillBaseReg + abi::kCycleCountOffset];

x64Host::x64Host(Context &context, Options::Compiler &options, std::pmr::memory_resource &alloc)
    : Host(context, options)
    , m_codeBuffer(new uint8_t[options.initialCodeBufferSize])
    , m_codeBufferSize(options.initialCodeBufferSize)
    , m_codegen(options.initialCodeBufferSize, m_codeBuffer.get())
    , m_alloc(alloc) {

    SetInvalidateCodeCacheCallback(
        [](uint32_t start, uint32_t end, void *ctx) {
            auto &host = *reinterpret_cast<x64Host *>(ctx);
            host.InvalidateCodeCacheRange(start, end);
        },
        this);

    m_codegen.setProtectMode(Xbyak::CodeGenerator::PROTECT_RWE);

    m_compiledCode.enableBlockLinking = options.enableBlockLinking;
    CompileCommon();
}

x64Host::~x64Host() {
    m_codegen.setProtectModeRW();
}

HostCode x64Host::Compile(ir::BasicBlock &block) {
    for (;;) {
        HostCode code = nullptr;
        try {
            // Try compiling the block
            code = CompileImpl(block);
        } catch (Xbyak::Error e) {
            if ((int)e == Xbyak::ERR_CODE_IS_TOO_BIG) {
                // If compilation fails due to filling up the code buffer, double its size and try compiling again
                m_codeBufferSize *= 2;
                m_codeBuffer.reset(new uint8_t[m_codeBufferSize]);
                m_codegen.setCodeBuffer(m_codeBuffer.get(), m_codeBufferSize);
                m_codegen.setProtectMode(Xbyak::CodeGenerator::PROTECT_RWE);
                Clear();
            } else {
                // Otherwise, rethrow exception
                throw;
            }
        }
        if (code != nullptr) {
            return code;
        }
    }
}

void x64Host::Clear() {
    m_compiledCode.Clear();
    m_codegen.reset();
    m_compiledCode.enableBlockLinking = m_options.enableBlockLinking;

    CompileCommon();
}

void x64Host::InvalidateCodeCache() {
    m_compiledCode.blockCache.clear();
    m_compiledCode.pendingPatches.clear();
    m_compiledCode.appliedPatches.clear();
}

void x64Host::InvalidateCodeCacheRange(uint32_t start, uint32_t end) {
    if (start == 0 && end == 0xFFFFFFFF) {
        InvalidateCodeCache();
        return;
    }

    // TODO: make this more efficient
    // - consider redesigning the cache -> CPSR in the lower bits
    for (uint64_t cpsr = 0; cpsr < 63; cpsr++) {
        const uint64_t upper = cpsr << 32ull;
        auto it = m_compiledCode.blockCache.lower_bound(start | upper);
        while (it != m_compiledCode.blockCache.end() && it->first >= (start | upper) && it->first <= (end | upper)) {
            if (m_compiledCode.enableBlockLinking) {
                // Undo patches
                RevertDirectLinkPatches(it->first);

                // Remove any pending patches for this block
                m_compiledCode.pendingPatches.erase(it->first);
            }

            // Remove the block from the cache
            it = m_compiledCode.blockCache.erase(it);
        }
    }
}

void x64Host::CompileCommon() {
    CompileEpilog();
    CompileIRQEntry();
    CompileProlog(); // Depends on Epilog and IRQEntry being compiled
}

void x64Host::CompileProlog() {
    auto &armState = m_context.GetARMState();

    m_compiledCode.prolog = m_codegen.getCurr<CompiledCode::PrologFn>();

    // -----------------------------------------------------------------------------------------------------------------
    // Entry setup

    // Push all nonvolatile registers
    for (auto &reg : abi::kNonvolatileRegs) {
        m_codegen.push(reg);
    }

    // Setup static registers and stack
    // Make space for variable spill area + cycle counter, including the stack alignment offset
    m_codegen.sub(rsp, abi::kStackReserveSize);
    m_codegen.mov(abi::kVarSpillBaseReg, rsp);                // rbp = Variable spill and cycle counter base register
    m_codegen.mov(abi::kARMStateReg, CastUintPtr(&armState)); // rbx = ARM state pointer

    // Copy cycle count to its slot in the stack (2nd argument passed to prolog function)
    m_codegen.mov(kCycleCountOperand, abi::kIntArgRegs[1]);

    // Copy CPSR NZCV and I flags to EAX
    auto flagsReg32 = abi::kHostFlagsReg;
    auto iFlagReg32 = r15d;
    m_codegen.mov(flagsReg32, dword[CastUintPtr(&armState.CPSR())]);
    m_codegen.mov(iFlagReg32, flagsReg32);
    m_codegen.and_(iFlagReg32, (1 << ARMflgIPos));      // Keep I flag
    m_codegen.shl(iFlagReg32, x64flgIPos - ARMflgIPos); // Shift I flag to correct place
    m_codegen.shr(flagsReg32, ARMflgNZCVShift);         // Shift NZCV bits to [3..0]
    if (CPUID::HasFastPDEPAndPEXT()) {
        // AH       AL
        // SZ0A0P1C -------V
        // NZ.....C .......V
        auto depMaskReg32 = r14d;
        m_codegen.mov(depMaskReg32, 0b11000001'00000001u); // Deposit bit mask: NZ-----C -------V
        m_codegen.pdep(flagsReg32, flagsReg32, depMaskReg32);
    } else {
        m_codegen.imul(flagsReg32, flagsReg32, ARMTox64FlagsMult); // -------- -------- NZCV-NZC V---NZCV
        m_codegen.and_(flagsReg32, x64FlagsMask);                  // -------- -------- NZ-----C -------V
    }
    m_codegen.or_(flagsReg32, iFlagReg32); // -------- -------I NZ-----C -------V

    // -----------------------------------------------------------------------------------------------------------------
    // Execution state check

    {
        Xbyak::Label lblContinue{};

        // Check if the CPU is running
        const auto execStateOfs = m_stateOffsets.ExecutionStateOffset();
        m_codegen.cmp(byte[abi::kARMStateReg + execStateOfs], static_cast<uint8_t>(arm::ExecState::Running));
        m_codegen.je(lblContinue);

        // At this point, the CPU is halted
        {
            // Check if IRQ line is asserted
            const auto irqLineOffset = m_stateOffsets.IRQLineOffset();
            m_codegen.test(byte[abi::kARMStateReg + irqLineOffset], 1);

            // Exit if not asserted
            m_codegen.jz(m_compiledCode.epilog);

            // IRQ line is asserted
            {
                // Change execution state to Running
                m_codegen.mov(byte[abi::kARMStateReg + execStateOfs], static_cast<uint8_t>(arm::ExecState::Running));

                // Jump to IRQ vector if not suppressed by CPSR I bit
                m_codegen.test(abi::kHostFlagsReg, x64flgI);
                m_codegen.jz(m_compiledCode.irqEntry);

                // Otherwise, fallthrough and jump to block
            }
        }

        // CPU is running
        m_codegen.L(lblContinue);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Block execution

    // Jump to block code
    m_codegen.jmp(abi::kIntArgRegs[0]);

    vtune::ReportCode(CastUintPtr(m_compiledCode.prolog), m_codegen.getCurr<uintptr_t>(), "__prolog");
}

void x64Host::CompileEpilog() {
    m_compiledCode.epilog = m_codegen.getCurr<HostCode>();

    // Copy remaining cycles to return value
    m_codegen.mov(abi::kIntReturnValueReg, kCycleCountOperand);

    // Cleanup stack
    m_codegen.add(rsp, abi::kStackReserveSize);

    // Pop all nonvolatile registers
    for (auto it = abi::kNonvolatileRegs.rbegin(); it != abi::kNonvolatileRegs.rend(); it++) {
        m_codegen.pop(*it);
    }

    // Return from call
    m_codegen.ret();

    vtune::ReportCode(CastUintPtr(m_compiledCode.epilog), m_codegen.getCurr<uintptr_t>(), "__epilog");
}

void x64Host::CompileIRQEntry() {
    m_compiledCode.irqEntry = m_codegen.getCurr<HostCode>();

    auto &armState = m_context.GetARMState();

    // Get temporary registers for operations
    auto pcReg32 = abi::kIntArgRegs[0].cvt32();
    auto cpsrReg32 = abi::kIntArgRegs[1].cvt32(); // Also used as 2nd argument to function call
    auto lrReg32 = abi::kIntArgRegs[2].cvt32();

    // Get field offsets
    const auto cpsrOffset = m_stateOffsets.CPSROffset();
    const auto spsrOffset = m_stateOffsets.SPSROffset(arm::Mode::IRQ);
    const auto pcOffset = m_stateOffsets.GPROffset(arm::GPR::PC, arm::Mode::User);
    const auto lrOffset = m_stateOffsets.GPROffset(arm::GPR::LR, arm::Mode::IRQ);
    const auto execStateOffset = m_stateOffsets.ExecutionStateOffset();

    // -----------------------------------------------------------------------------------------------------------------
    // IRQ exception vector entry

    // Use PC register as temporary storage for CPSR to avoid two memory reads
    m_codegen.mov(pcReg32, dword[abi::kARMStateReg + cpsrOffset]);

    // Copy CPSR to SPSR_irq
    m_codegen.mov(cpsrReg32, pcReg32);
    m_codegen.mov(dword[abi::kARMStateReg + spsrOffset], cpsrReg32);

    // Set LR
    m_codegen.test(cpsrReg32, (1u << ARMflgTPos)); // Test against CPSR T bit
    m_codegen.setne(cpsrReg32.cvt8());             // cpsrReg32 will be 1 in Thumb mode
    m_codegen.movzx(cpsrReg32, cpsrReg32.cvt8());
    m_codegen.mov(lrReg32, dword[abi::kARMStateReg + pcOffset]); // LR = PC
    m_codegen.lea(lrReg32, dword[lrReg32 + cpsrReg32 * 4 - 4]);  // LR = PC + 0 (Thumb)
    m_codegen.mov(dword[abi::kARMStateReg + lrOffset], lrReg32); // LR = PC - 4 (ARM)

    // Modify CPSR T and mode bits
    constexpr uint32_t setBits = static_cast<uint32_t>(arm::Mode::IRQ) | (1u << ARMflgIPos);
    m_codegen.mov(cpsrReg32, pcReg32);
    m_codegen.and_(cpsrReg32, ~0b11'1111);
    m_codegen.or_(cpsrReg32, setBits);
    m_codegen.mov(dword[abi::kARMStateReg + cpsrOffset], cpsrReg32);

    // Update I in EAX
    m_codegen.and_(cpsrReg32, (1 << ARMflgIPos));
    m_codegen.and_(abi::kHostFlagsReg, ~x64flgI);
    m_codegen.shl(cpsrReg32, x64flgIPos - ARMflgIPos);
    m_codegen.or_(abi::kHostFlagsReg, cpsrReg32);

    // Set PC
    constexpr uint32_t irqVectorOffset =
        (2u + static_cast<uint32_t>(arm::Exception::NormalInterrupt)) * sizeof(uint32_t);
    auto &cp15 = armState.GetSystemControlCoprocessor();
    if (cp15.IsPresent()) {
        // Load base vector address from CP15
        auto &cp15ctl = cp15.GetControlRegister();
        const auto baseVectorAddressOfs = offsetof(arm::cp15::ControlRegister, baseVectorAddress);
        m_codegen.mov(pcReg32.cvt64(), CastUintPtr(&cp15ctl));
        m_codegen.mov(pcReg32, dword[pcReg32.cvt64() + baseVectorAddressOfs]);
        m_codegen.add(pcReg32, irqVectorOffset);
    } else {
        // Assume 00000000 if CP15 is absent
        m_codegen.mov(pcReg32, irqVectorOffset);
    }
    m_codegen.mov(dword[abi::kARMStateReg + pcOffset], pcReg32);

    // Clear halt state
    m_codegen.mov(byte[abi::kARMStateReg + execStateOffset], static_cast<uint8_t>(arm::ExecState::Running));

    // Count cycles
    m_codegen.sub(kCycleCountOperand, 1);

    // -----------------------------------------------------------------------------------------------------------------
    // IRQ handler block linking

    if (m_options.enableBlockLinking) {
        // Build cache key
        auto cacheKeyReg64 = cpsrReg32.cvt64();
        m_codegen.mov(cacheKeyReg64, dword[abi::kARMStateReg + cpsrOffset]);
        m_codegen.and_(cacheKeyReg64, 0x3F); // We only need the mode and T bits
        m_codegen.shl(cacheKeyReg64, 32);
        m_codegen.or_(cacheKeyReg64, pcReg32.cvt64());

        // Save return register
        m_codegen.push(abi::kIntReturnValueReg);
        constexpr uint64_t volatileRegsSize = (1 + 1) * sizeof(uint64_t);
        constexpr uint64_t stackAlignmentOffset =
            abi::Align<abi::kStackAlignmentShift>(volatileRegsSize) - volatileRegsSize;

        // Lookup entry in block cache
        // TODO: redesign cache to not rely on this function call
        m_codegen.mov(abi::kIntArgRegs[0], CastUintPtr(&m_compiledCode.blockCache)); // 1st argument
        m_codegen.mov(abi::kIntReturnValueReg, CastUintPtr(CompiledCode::GetCodeForLocationTrampoline));
        if (stackAlignmentOffset != 0) {
            m_codegen.sub(rsp, stackAlignmentOffset);
        }
        m_codegen.call(abi::kIntReturnValueReg);
        if (stackAlignmentOffset != 0) {
            m_codegen.add(rsp, stackAlignmentOffset);
        }

        // Jump to block if present, or epilog if not
        m_codegen.test(abi::kIntReturnValueReg, abi::kIntReturnValueReg);
        m_codegen.mov(cpsrReg32.cvt64(), CastUintPtr(m_compiledCode.epilog));
        m_codegen.cmovnz(cpsrReg32.cvt64(), abi::kIntReturnValueReg);
        m_codegen.pop(abi::kIntReturnValueReg); // Restore return register
        m_codegen.jmp(cpsrReg32.cvt64());
    } else {
        // Jump to epilog if block linking is disabled.
        // This allows the dispatcher to see and react to the IRQ entry.
        m_codegen.jmp(m_compiledCode.epilog);
    }

    vtune::ReportCode(CastUintPtr(m_compiledCode.irqEntry), m_codegen.getCurr<uintptr_t>(), "__irqEntry");
}

HostCode x64Host::CompileImpl(ir::BasicBlock &block) {
    auto &cachedBlock = m_compiledCode.blockCache[block.Location().ToUint64()];
    Compiler compiler{m_context, m_compiledCode, m_codegen, block, m_alloc};

    auto fnPtr = m_codegen.getCurr<HostCode>();
    cachedBlock.code = fnPtr;

    Xbyak::Label lblCondFail{};

    // Compile pre-execution checks
    compiler.CompileIRQLineCheck();
    compiler.CompileCondCheck(block.Condition(), lblCondFail);

    // Compile block code
    if (block.Condition() != arm::Condition::NV) {
        auto *op = block.Head();
        while (op != nullptr) {
            compiler.PreProcessOp(op);
            ir::VisitIROp(op, [&compiler](const auto *op) -> void { compiler.CompileOp(op); });
            compiler.PostProcessOp(op);
            op = op->Next();
        }

        // Decrement cycles for this block
        // TODO: proper cycle counting
        m_codegen.sub(kCycleCountOperand, block.InstructionCount());

        // Bail out if we ran out of cycles
        m_codegen.jle(m_compiledCode.epilog);
    }

    if (block.Condition() != arm::Condition::AL) {
        // Go to next block or epilog
        compiler.CompileTerminal(block);

        // Condition fail block
        {
            m_codegen.L(lblCondFail);

            // Update PC if condition fails
            const auto pcRegOffset = m_stateOffsets.GPROffset(arm::GPR::PC, block.Location().Mode());
            const uint32_t instrSize = block.Location().IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t);
            m_codegen.mov(dword[abi::kARMStateReg + pcRegOffset],
                          block.Location().PC() + block.InstructionCount() * instrSize);

            // Decrement cycles for this block's condition fail branch
            // TODO: properly decrement cycles for failing the check
            m_codegen.sub(kCycleCountOperand, block.InstructionCount());

            if (m_compiledCode.enableBlockLinking) {
                // Bail out if we ran out of cycles
                m_codegen.jle(m_compiledCode.epilog);

                // Link to next instruction
                compiler.CompileDirectLinkToSuccessor(block);
            } else {
                // Bail out immediately if block linking is disabled
                m_codegen.jmp(m_compiledCode.epilog);
            }
        }
    }

    // Go to next block or epilog
    compiler.CompileTerminal(block);

    // -----------------------------------------------------------------------------------------------------------------

    if (m_compiledCode.enableBlockLinking) {
        // Patch references to this block
        ApplyDirectLinkPatches(block.Location(), fnPtr);
    }

    // Cleanup, cache block and return pointer to code
    vtune::ReportBasicBlock(CastUintPtr(fnPtr), m_codegen.getCurr<uintptr_t>(), block.Location());
    return fnPtr;
}

void x64Host::ApplyDirectLinkPatches(LocationRef target, HostCode blockCode) {
    const uint64_t key = target.ToUint64();
    auto itPatch = m_compiledCode.pendingPatches.find(key);
    while (itPatch != m_compiledCode.pendingPatches.end() && itPatch->first == key) {
        auto &patchInfo = itPatch->second;
        auto itPatchBlock = m_compiledCode.blockCache.find(patchInfo.cachedBlockKey);
        if (itPatchBlock != m_compiledCode.blockCache.end()) {
            // Remember current location
            auto prevSize = m_codegen.getSize();

            // Go to patch location
            m_codegen.setSize(patchInfo.codePos - m_codegen.getCode());

            // If target is close enough, emit up to three NOPs, otherwise emit a JMP to the target address
            auto distToTarget = (const uint8_t *)blockCode - patchInfo.codePos;
            if (distToTarget >= 1 && distToTarget <= 27 && blockCode == patchInfo.codeEnd) {
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
                m_codegen.jmp(blockCode, Xbyak::CodeGenerator::T_NEAR);
            }

            // Restore code generator position
            m_codegen.setSize(prevSize);
        }

        // Move patch to the applied patches list
        m_compiledCode.appliedPatches.insert({key, patchInfo});

        // Remove the patch from the pending list
        itPatch = m_compiledCode.pendingPatches.erase(itPatch);
    }
}

void x64Host::RevertDirectLinkPatches(uint64_t key) {
    auto itPatch = m_compiledCode.appliedPatches.find(key);
    while (itPatch != m_compiledCode.appliedPatches.end() && itPatch->first == key) {
        auto &patchInfo = itPatch->second;
        auto itPatchBlock = m_compiledCode.blockCache.find(patchInfo.cachedBlockKey);
        if (itPatchBlock != m_compiledCode.blockCache.end()) {
            // Remember current location
            auto prevSize = m_codegen.getSize();

            // Go to patch location
            m_codegen.setSize(patchInfo.codePos - m_codegen.getCode());

            // Overwrite with a jump to the epilog
            m_codegen.jmp(m_compiledCode.epilog, Xbyak::CodeGenerator::T_NEAR);

            // Restore code generator position
            m_codegen.setSize(prevSize);
        }

        // Remove the patch from the applied list
        itPatch = m_compiledCode.appliedPatches.erase(itPatch);
    }
}

} // namespace armajitto::x86_64
