#include "armajitto/host/x86_64/x86_64_host.hpp"

#include "armajitto/guest/arm/exceptions.hpp"
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

    // Calculate reserved stack size
    uint64_t stackSize = abi::kNonvolatileRegs.size() * sizeof(uint64_t);
    stackSize += sizeof(uint64_t); // +1 for RIP pushed by call

    // Calculate offset needed to compensate for stack misalignment
    m_stackAlignmentOffset = abi::Align<abi::kStackAlignmentShift>(stackSize) - stackSize;

    CompileCommon();
}

HostCode x64Host::Compile(ir::BasicBlock &block) {
    auto &cachedBlock = m_compiledCode.blockCache[block.Location().ToUint64()];
    Compiler compiler{m_context, m_compiledCode, m_codegen, block};

    auto fnPtr = m_codegen.getCurr<HostCode>();
    m_codegen.setProtectModeRW();

    Xbyak::Label lblCondFail{};
    Xbyak::Label lblCondPass{};

    compiler.CompileIRQLineCheck();
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
    m_codegen.jmp(lblCondPass);

    // Update PC if condition fails
    m_codegen.L(lblCondFail);
    const auto pcRegOffset = m_context.GetARMState().GPROffset(arm::GPR::PC, block.Location().Mode());
    const auto instrSize = block.Location().IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t);
    m_codegen.mov(dword[abi::kARMStateReg + pcRegOffset], block.Location().PC() + block.InstructionCount() * instrSize);
    // TODO: increment cycles for failing the check

    m_codegen.L(lblCondPass);

    // TODO: cycle counting
    // TODO: check cycles

    // Go to next block or epilog
    compiler.CompileTerminal(block);

    m_codegen.setProtectModeRE();

    cachedBlock.code = fnPtr;
    vtune::ReportBasicBlock(fnPtr, m_codegen.getCurr<uintptr_t>(), block.Location());

    // Patch references to this block
    compiler.PatchIndirectLinks(block.Location(), fnPtr);

    return fnPtr;
}

void x64Host::Clear() {
    m_compiledCode.Clear();
    m_codegen.resetAndReallocate();

    CompileCommon();
}

void x64Host::CompileCommon() {
    CompileEpilog();
    CompileEnterIRQ();
    CompileProlog(); // Depends on Epilog and ExitIRQ being compiled
}

void x64Host::CompileProlog() {
    auto &armState = m_context.GetARMState();

    m_compiledCode.prolog = m_codegen.getCurr<CompiledCode::PrologFn>();
    m_codegen.setProtectModeRW();

    // -----------------------------------------------------------------------------------------------------------------
    // Entry setup

    // Push all nonvolatile registers
    for (auto &reg : abi::kNonvolatileRegs) {
        m_codegen.push(reg);
    }

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

    // Setup other static registers
    m_codegen.mov(abi::kARMStateReg, CastUintPtr(&armState)); // ARM state pointer

    // -----------------------------------------------------------------------------------------------------------------
    // Execution state check

    // Check if the CPU is running
    Xbyak::Label lblContinue{};
    const auto execStateOfs = armState.ExecutionStateOffset();
    m_codegen.cmp(byte[abi::kARMStateReg + execStateOfs], static_cast<uint8_t>(arm::ExecState::Running));
    m_codegen.je(lblContinue);

    // At this point, the CPU is halted

    // Get and invert CPSR I bit
    const auto cpsrOffset = armState.CPSROffset();
    auto tmpReg8 = abi::kNonvolatileRegs.back().cvt8();
    m_codegen.test(dword[abi::kARMStateReg + cpsrOffset], (1u << ARMflgIPos));
    m_codegen.sete(tmpReg8);

    // Compare against IRQ line
    const auto irqLineOffset = armState.IRQLineOffset();
    m_codegen.test(byte[abi::kARMStateReg + irqLineOffset], tmpReg8);

    // Exit if IRQ is not raised
    // TODO: set "executed" cycles to requested cycles
    m_codegen.jz((void *)m_compiledCode.epilog);

    // Change execution state to Running otherwise, and enter IRQ
    m_codegen.mov(byte[abi::kARMStateReg + execStateOfs], static_cast<uint8_t>(arm::ExecState::Running));
    m_codegen.jmp((void *)m_compiledCode.enterIRQ);

    // -----------------------------------------------------------------------------------------------------------------
    // Block execution

    // Call block function
    auto funcAddr = abi::kNonvolatileRegs.back();
    m_codegen.L(lblContinue);
    m_codegen.mov(funcAddr, abi::kIntArgRegs[0]); // Get block code pointer from 1st arg
    m_codegen.jmp(funcAddr);                      // Jump to block code

    m_codegen.setProtectModeRE();
    vtune::ReportCode(CastUintPtr(m_compiledCode.prolog), m_codegen.getCurr<uintptr_t>(), "__prolog");
}

void x64Host::CompileEpilog() {
    m_compiledCode.epilog = m_codegen.getCurr<HostCode>();
    m_codegen.setProtectModeRW();

    // Cleanup stack
    m_codegen.add(rsp, abi::kStackReserveSize + m_stackAlignmentOffset);

    // Pop all nonvolatile registers
    for (auto it = abi::kNonvolatileRegs.rbegin(); it != abi::kNonvolatileRegs.rend(); it++) {
        m_codegen.pop(*it);
    }

    // TODO: Put executed/remaining/whatever cycles in return value
    // m_codegen.mov(abi::kIntReturnValueReg.cvt32(), <executed cycles>);

    // Return from call
    m_codegen.ret();

    m_codegen.setProtectModeRE();
    vtune::ReportCode(m_compiledCode.epilog, m_codegen.getCurr<uintptr_t>(), "__epilog");
}

void x64Host::CompileEnterIRQ() {
    m_compiledCode.enterIRQ = m_codegen.getCurr<HostCode>();
    m_codegen.setProtectModeRW();

    auto &armState = m_context.GetARMState();

    // Get temporary registers for operations
    auto pcReg32 = abi::kIntArgRegs[0].cvt32();
    auto cpsrReg32 = abi::kIntArgRegs[1].cvt32(); // Also used as 2nd argument to function call
    auto lrReg32 = abi::kIntArgRegs[2].cvt32();
    auto lrOffsetReg32 = abi::kIntArgRegs[3].cvt32();

    // Get field offsets
    const auto cpsrOffset = armState.CPSROffset();
    const auto spsrOffset = armState.SPSROffset(arm::Mode::IRQ);
    const auto pcOffset = armState.GPROffset(arm::GPR::PC, arm::Mode::User);
    const auto baseLROffset = armState.GPROffsetsOffset() + static_cast<size_t>(arm::GPR::LR) * sizeof(uintptr_t);
    const auto execStateOffset = armState.ExecutionStateOffset();

    // -----------------------------------------------------------------------------------------------------------------
    // IRQ exception vector entry

    // Use PC register as temporary storage for CPSR to avoid two memory reads
    m_codegen.mov(pcReg32, dword[abi::kARMStateReg + cpsrOffset]);

    // Copy CPSR to SPSR of the IRQ's mode
    m_codegen.mov(cpsrReg32, pcReg32);
    m_codegen.mov(dword[abi::kARMStateReg + spsrOffset], cpsrReg32);

    // Get LR offset based on current CPSR mode (using State::m_gprOffsets)
    m_codegen.mov(lrOffsetReg32, cpsrReg32);
    m_codegen.and_(lrOffsetReg32, 0x1F); // Extract CPSR mode bits
    m_codegen.shl(lrOffsetReg32, 4);     // Multiply by 16
    m_codegen.lea(lrOffsetReg32, dword[lrOffsetReg32 * sizeof(uintptr_t) + baseLROffset]);
    m_codegen.mov(lrOffsetReg32, dword[abi::kARMStateReg + lrOffsetReg32.cvt64()]);

    // Set LR
    m_codegen.mov(lrReg32, dword[abi::kARMStateReg + pcOffset]); // LR = PC
    m_codegen.test(cpsrReg32, (1u << ARMflgTPos));               // Test against CPSR T bit
    m_codegen.setne(cpsrReg32.cvt8());                           // cpsrReg32 will be 1 in Thumb mode
    m_codegen.movzx(cpsrReg32, cpsrReg32.cvt8());
    m_codegen.lea(lrReg32, dword[lrReg32 + cpsrReg32 * 4 - 4]);               // LR = PC + 0 (Thumb)
    m_codegen.mov(dword[abi::kARMStateReg + lrOffsetReg32.cvt64()], lrReg32); // LR = PC - 4 (ARM)

    // Modify CPSR T and mode bits
    constexpr uint32_t setBits = static_cast<uint32_t>(arm::Mode::IRQ) | (1u << ARMflgIPos);
    m_codegen.mov(cpsrReg32, pcReg32);
    m_codegen.and_(cpsrReg32, ~0b11'1111);
    m_codegen.or_(cpsrReg32, setBits);
    m_codegen.mov(dword[abi::kARMStateReg + cpsrOffset], cpsrReg32);

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

    // -----------------------------------------------------------------------------------------------------------------
    // IRQ handler block linking

    // Build cache key
    auto cacheKeyReg64 = cpsrReg32.cvt64(); // 2nd argument to function call, which already contains CPSR
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
    m_codegen.mov(cpsrReg32.cvt64(), m_compiledCode.epilog);
    m_codegen.cmovnz(cpsrReg32.cvt64(), abi::kIntReturnValueReg);
    m_codegen.pop(abi::kIntReturnValueReg); // Restore return register
    m_codegen.jmp(cpsrReg32.cvt64());

    m_codegen.setProtectModeRE();
}

} // namespace armajitto::x86_64
