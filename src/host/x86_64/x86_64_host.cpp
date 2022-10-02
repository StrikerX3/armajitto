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

struct x64Host::CommonData {
    arm::StateOffsets stateOffsets;

    CommonData(arm::State &state)
        : stateOffsets(state) {}
};

x64Host::x64Host(Context &context, Options::Compiler &options, std::pmr::memory_resource &alloc)
    : Host(context, options)
    , m_commonData(std::make_unique<CommonData>(context.GetARMState()))
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
                const auto prevCodeBufferSize = m_codeBufferSize;
                m_codeBufferSize *= 2;
                if (m_codeBufferSize > m_options.maximumCodeBufferSize) {
                    m_codeBufferSize = m_options.maximumCodeBufferSize;
                }
                if (prevCodeBufferSize != m_codeBufferSize) {
                    m_codeBuffer.reset(new uint8_t[m_codeBufferSize]);
                    m_codegen.setCodeBuffer(m_codeBuffer.get(), m_codeBufferSize);
                    m_codegen.setProtectMode(Xbyak::CodeGenerator::PROTECT_RWE);
                }
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

void x64Host::Invalidate(LocationRef loc) {
    const uint64_t key = loc.ToUint64();
    auto *block = m_compiledCode.blockCache.Get(key);
    if (block == nullptr) {
        return;
    }

    if (m_compiledCode.enableBlockLinking) {
        // Undo patches
        RevertDirectLinkPatches(key);
    }

    // Remove the block from the cache
    *block = nullptr;
}

void x64Host::InvalidateCodeCache() {
    m_compiledCode.blockCache.Clear();
    m_compiledCode.pendingPatches.clear();
    m_compiledCode.appliedPatches.clear();
}

void x64Host::InvalidateCodeCacheRange(uint32_t start, uint32_t end) {
    if (start == 0 && end == 0xFFFFFFFF) {
        InvalidateCodeCache();
        return;
    }

    // TODO: make this more efficient
    // - consider moving CPSR to the lower bits of the key
    for (uint64_t cpsr = 0; cpsr < 63; cpsr++) {
        const uint64_t upper = cpsr << 32ull;
        for (uint64_t addr = start; addr <= end; addr += 2) {
            const uint64_t key = addr | upper;
            auto *block = m_compiledCode.blockCache.Get(key);
            if (block == nullptr || *block == nullptr) {
                continue;
            }

            if (m_compiledCode.enableBlockLinking) {
                // Undo patches
                RevertDirectLinkPatches(key);
            }

            // Remove the block from the cache
            *block = nullptr;
        }
    }
}

void x64Host::ReportMemoryWrite(uint32_t start, uint32_t end) {
    // Increment memory generations for all affected pages
    const uint32_t basePage = start >> CompiledCode::kPageShift;
    const uint32_t finalPage = end >> CompiledCode::kPageShift;
    for (uint32_t page = basePage; page <= finalPage; page++) {
        ++m_compiledCode.memPageGenerations[page];
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
    m_codegen.mov(abi::kCycleCountReg, abi::kIntArgRegs[1]);  // r10 = remaining cycle count

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
    m_codegen.mov(abi::kIntReturnValueReg, abi::kCycleCountReg);

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
    m_codegen.or_(abi::kHostFlagsReg, x64flgI);

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
    // TODO: compute cycles for pipeline refill
    m_codegen.dec(abi::kCycleCountReg);

    // -----------------------------------------------------------------------------------------------------------------
    // IRQ handler block linking

    if (m_options.enableBlockLinking) {
        auto cpsrReg64 = cpsrReg32.cvt64();
        auto pcReg64 = pcReg32.cvt64();
        auto lrReg64 = lrReg32.cvt64();

        // Build cache key
        m_codegen.mov(cpsrReg64.cvt32(), dword[abi::kARMStateReg + cpsrOffset]);
        m_codegen.and_(cpsrReg64, 0x3F); // We only need the mode and T bits
        m_codegen.shl(cpsrReg64, 32);
        m_codegen.or_(cpsrReg64, pcReg32.cvt64());

        // Lookup entry in block cache
        m_codegen.mov(pcReg64, m_compiledCode.blockCache.MapAddress());
        m_codegen.mov(pcReg64, qword[pcReg64]);

        using CacheType = decltype(m_compiledCode.blockCache);

        // Level 1 check
        m_codegen.mov(lrReg64, cpsrReg64);
        m_codegen.shr(lrReg64, CacheType::kL1Shift);
        // m_codegen.and_(lrReg64, CacheType::kL1Mask); // shouldn't be necessary
        m_codegen.mov(pcReg64, qword[pcReg64 + lrReg64 * sizeof(void *)]);
        m_codegen.test(pcReg64, pcReg64);
        m_codegen.jz(m_compiledCode.epilog);

        // Level 2 check
        m_codegen.mov(lrReg64, cpsrReg64);
        m_codegen.shr(lrReg64, CacheType::kL2Shift);
        m_codegen.and_(lrReg64, CacheType::kL2Mask);
        m_codegen.mov(pcReg64, qword[pcReg64 + lrReg64 * sizeof(void *)]);
        m_codegen.test(pcReg64, pcReg64);
        m_codegen.jz(m_compiledCode.epilog);

        // Level 3 check
        // m_codegen.shr(cpsrReg64, CacheType::kL3Shift); // shift by zero
        m_codegen.and_(cpsrReg64, CacheType::kL3Mask);
        static constexpr auto valueSize = CacheType::kValueSize;
        if constexpr (valueSize >= 1 && valueSize <= 8 && std::popcount(valueSize) == 1) {
            m_codegen.mov(pcReg64, qword[pcReg64 + cpsrReg64 * valueSize]);
        } else {
            m_codegen.imul(cpsrReg64, cpsrReg64, valueSize);
            m_codegen.mov(pcReg64, qword[pcReg64 + cpsrReg64]);
        }

        // Jump to block if present, or epilog if not
        m_codegen.test(pcReg64, pcReg64);
        m_codegen.mov(cpsrReg64, CastUintPtr(m_compiledCode.epilog));
        m_codegen.cmovnz(cpsrReg64, pcReg64);
        m_codegen.jmp(cpsrReg64);
    } else {
        // Jump to epilog if block linking is disabled.
        // This allows the dispatcher to see and react to the IRQ entry.
        m_codegen.jmp(m_compiledCode.epilog);
    }

    vtune::ReportCode(CastUintPtr(m_compiledCode.irqEntry), m_codegen.getCurr<uintptr_t>(), "__irqEntry");
}

HostCode x64Host::CompileImpl(ir::BasicBlock &block) {
    auto &cachedBlock = m_compiledCode.blockCache.GetOrCreate(block.Location().ToUint64());
    Compiler compiler{m_context, m_commonData->stateOffsets, m_compiledCode, m_codegen, block, m_alloc};

    auto fnPtr = m_codegen.getCurr<HostCode>();
    cachedBlock = fnPtr;

    Xbyak::Label lblCondFail{};

    // Compile pre-execution checks
    compiler.CompileGenerationCheck(block.Location(), block.InstructionCount());
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
        compiler.SubtractCycles(block.PassCycles());

        // Bail out if we ran out of cycles
        m_codegen.jle(m_compiledCode.epilog);
    }

    if (block.Condition() != arm::Condition::AL) {
        if (block.Condition() != arm::Condition::NV) {
            // Go to next block or epilog
            compiler.CompileTerminal(block);
        } else {
            // Allow registers to be spilled if necessary
            // FIXME: this is hacky af... ideally the code generator should prepend all spilled registers
            compiler.ReserveTerminalRegisters(block);
        }

        // Condition fail block
        {
            m_codegen.L(lblCondFail);

            // Update PC if condition fails
            const auto pcRegOffset = m_stateOffsets.GPROffset(arm::GPR::PC, block.Location().Mode());
            const uint32_t instrSize = block.Location().IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t);
            m_codegen.mov(dword[abi::kARMStateReg + pcRegOffset],
                          block.Location().PC() + block.InstructionCount() * instrSize);

            // Decrement cycles for this block's condition fail branch
            compiler.SubtractCycles(block.FailCycles());

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
        auto patchBlock = m_compiledCode.blockCache.Get(patchInfo.cachedBlockKey);
        if (patchBlock != nullptr && *patchBlock != nullptr) {
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

        // Remember current location
        auto prevSize = m_codegen.getSize();

        // Go to patch location
        m_codegen.setSize(patchInfo.codePos - m_codegen.getCode());

        // Overwrite with a jump to the epilog
        m_codegen.jmp(m_compiledCode.epilog, Xbyak::CodeGenerator::T_NEAR);

        // Restore code generator position
        m_codegen.setSize(prevSize);

        // Add the patch to the pending patch list
        m_compiledCode.pendingPatches.insert({itPatch->first, itPatch->second});

        // Remove the patch from the applied list
        itPatch = m_compiledCode.appliedPatches.erase(itPatch);
    }
}

} // namespace armajitto::x86_64
