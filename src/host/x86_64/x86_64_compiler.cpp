#include "x86_64_compiler.hpp"

#include "guest/arm/arithmetic.hpp"

#include "util/bit_ops.hpp"
#include "util/unreachable.hpp"

#include "cpuid.hpp"
#include "x86_64_flags.hpp"

#include <bit>
#include <cassert>

namespace armajitto::x86_64 {

using namespace Xbyak::util;

// ---------------------------------------------------------------------------------------------------------------------
// System method accessor trampolines

// ARMv4T and ARMv5TE LDRB
static uint32_t SystemMemReadByte(ISystem &system, uint32_t address) {
    return system.MemReadByte(address);
}

// ARMv4T and ARMv5TE LDRSB
static uint32_t SystemMemReadSignedByte(ISystem &system, uint32_t address) {
    return bit::sign_extend<8, int32_t>(system.MemReadByte(address));
}

// ARMv4T LDRSH
static uint32_t SystemMemReadSignedHalfOrByte(ISystem &system, uint32_t address) {
    if (address & 1) {
        return bit::sign_extend<8, int32_t>(system.MemReadByte(address));
    } else {
        return bit::sign_extend<16, int32_t>(system.MemReadHalf(address));
    }
}

// ARMv5TE LDRSH
static uint32_t SystemMemReadSignedHalf(ISystem &system, uint32_t address) {
    return bit::sign_extend<16, int32_t>(system.MemReadHalf(address & ~1));
}

// ARMv4T LDRH
static uint32_t SystemMemReadUnalignedRotatedHalf(ISystem &system, uint32_t address) {
    uint16_t value = system.MemReadHalf(address & ~1);
    if (address & 1) {
        value = std::rotr(value, 8);
    }
    return value;
}

// ARMv5TE LDRH
static uint32_t SystemMemReadUnalignedHalf(ISystem &system, uint32_t address) {
    return system.MemReadHalf(address & ~1);
}

// ARMv5TE LDRH
static uint32_t SystemMemReadAlignedHalf(ISystem &system, uint32_t address) {
    return system.MemReadHalf(address & ~1);
}

// ARMv4T and ARMv5TE LDR r15
static uint32_t SystemMemReadAlignedWord(ISystem &system, uint32_t address) {
    return system.MemReadWord(address & ~3);
}

// ARMv4T and ARMv5TE LDR
static uint32_t SystemMemReadUnalignedWord(ISystem &system, uint32_t address) {
    uint32_t value = system.MemReadWord(address & ~3);
    return std::rotr(value, (address & 3) * 8);
}

// ARMv4T and ARMv5TE STRB
static void SystemMemWriteByte(ISystem &system, uint32_t address, uint32_t value) {
    system.MemWriteByte(address, value & 0xFF);
}

// ARMv4T and ARMv5TE STRH
static void SystemMemWriteHalf(ISystem &system, uint32_t address, uint32_t value) {
    system.MemWriteHalf(address & ~1, value & 0xFFFF);
}

// ARMv4T and ARMv5TE STR
static void SystemMemWriteWord(ISystem &system, uint32_t address, uint32_t value) {
    system.MemWriteWord(address & ~3, value);
}

// MRC
static uint32_t SystemLoadCopRegister(arm::State &state, uint32_t cpnum, uint32_t reg) {
    return state.GetCoprocessor(cpnum & 0xF).LoadRegister(reg & 0xFFFF);
}

// MCR
static void SystemStoreCopRegister(arm::State &state, uint32_t cpnum, uint32_t reg, uint32_t value) {
    return state.GetCoprocessor(cpnum & 0xF).StoreRegister(reg & 0xFFFF, value);
}

// MRC2
static uint32_t SystemLoadCopExtRegister(arm::State &state, uint32_t cpnum, uint32_t reg) {
    return state.GetCoprocessor(cpnum & 0xF).LoadExtRegister(reg & 0xFFFF);
}

// MCR2
static void SystemStoreCopExtRegister(arm::State &state, uint32_t cpnum, uint32_t reg, uint32_t value) {
    return state.GetCoprocessor(cpnum & 0xF).StoreExtRegister(reg & 0xFFFF, value);
}

// ---------------------------------------------------------------------------------------------------------------------

x64Host::Compiler::Compiler(Context &context, arm::StateOffsets &stateOffsets, CompiledCode &compiledCode,
                            Xbyak::CodeGenerator &codegen, const ir::BasicBlock &block,
                            std::pmr::memory_resource &alloc)
    : m_regAlloc(codegen, alloc)
    , m_context(context)
    , m_compiledCode(compiledCode)
    , m_armState(context.GetARMState())
    , m_stateOffsets(stateOffsets)
    , m_codegen(codegen)
    , m_memMap(context.GetSystem().GetMemoryMap()) {

    m_regAlloc.Analyze(block);
    m_mode = block.Location().Mode();
    m_thumb = block.Location().IsThumbMode();
}

void x64Host::Compiler::PreProcessOp(const ir::IROp *op) {
    m_regAlloc.SetInstruction(op);
}

void x64Host::Compiler::PostProcessOp(const ir::IROp *op) {
    m_regAlloc.ReleaseVars();
    m_regAlloc.ReleaseTemporaries();
}

void x64Host::Compiler::CompileGenerationCheck(const LocationRef &baseLoc, const uint32_t instrCount) {
    const uint32_t instrSize = baseLoc.IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t);
    const uint32_t baseAddress = baseLoc.PC() - instrSize * 2;
    const uint32_t finalAddress = baseAddress + instrSize * instrCount - 1;

    auto basePtrReg64 = m_regAlloc.GetTemporary().cvt64();
    auto l2BasePtrReg64 = m_regAlloc.GetTemporary().cvt64();
    auto l3BasePtrReg64 = m_regAlloc.GetTemporary().cvt64();

    Xbyak::Label lblContinue{};
    Xbyak::Label lblMismatch{};

    // Check the generation tracker for all entries corresponding to this block
    using MGT = MemoryGenerationTracker;
    auto &mgt = m_compiledCode.memGenTracker;
    m_codegen.mov(basePtrReg64, mgt.MapAddress());

    bool maskSetUp = false;

    const uint32_t baseIndex1 = MGT::Level1Index(baseAddress);
    const uint32_t finalIndex1 = MGT::Level1Index(finalAddress);
    for (uint32_t index1 = baseIndex1; index1 <= finalIndex1; index1++) {
        const uint32_t addr1 = index1 << MGT::kL1Shift;
        const auto entry1 = mgt.Get(addr1);
        if (entry1.level == 1) {
            m_codegen.cmp(byte[basePtrReg64 + index1 * 8 + 7], entry1.counter); // check generation
            m_codegen.jne(lblMismatch, Xbyak::CodeGenerator::T_NEAR);           // mismatch; bail out
        } else {
            if (!maskSetUp) {
                m_codegen.mov(rcx, ~0xFF000000'00000000); // setup mask to fixup pointers
                maskSetUp = true;
            }
            m_codegen.mov(l2BasePtrReg64, qword[basePtrReg64 + index1 * 8]); // get level 2 pointer
            m_codegen.and_(l2BasePtrReg64, rcx);                             // clear top bits

            const uint32_t baseAddress1 = std::max(baseAddress, index1 << MGT::kL1Shift);
            const uint32_t finalAddress1 = std::min(finalAddress, baseAddress1 + (1 << MGT::kL1Shift));

            const uint32_t baseIndex2 = MGT::Level2Index(baseAddress1);
            const uint32_t finalIndex2 = MGT::Level2Index(finalAddress1);

            for (uint32_t index2 = baseIndex2; index2 <= finalIndex2; index2++) {
                const uint32_t addr2 = addr1 | (index2 << MGT::kL2Shift);
                const auto entry2 = mgt.Get(addr2);
                if (entry2.level == 2) {
                    m_codegen.cmp(byte[l2BasePtrReg64 + index2 * 8 + 7], entry2.counter); // check generation
                    m_codegen.jne(lblMismatch, Xbyak::CodeGenerator::T_NEAR);             // mismatch; bail out
                } else {
                    m_codegen.mov(l3BasePtrReg64, qword[l2BasePtrReg64 + index2 * 8]); // get level 3 pointer
                    m_codegen.and_(l3BasePtrReg64, rcx);                               // clear top bits

                    const uint32_t baseAddress2 = std::max(baseAddress, index2 << MGT::kL2Shift);
                    const uint32_t finalAddress2 = std::min(finalAddress, baseAddress2 + (1 << MGT::kL2Shift));

                    const uint32_t baseIndex3 = MGT::Level3Index(baseAddress2);
                    const uint32_t finalIndex3 = MGT::Level3Index(finalAddress2);

                    for (uint32_t index3 = baseIndex3; index3 <= finalIndex3; index3++) {
                        const uint32_t addr3 = addr2 | (index3 << MGT::kL3Shift);
                        const auto entry3 = mgt.Get(addr3);
                        m_codegen.cmp(dword[l3BasePtrReg64 + index3 * 4], entry3.counter);
                        m_codegen.jne(lblMismatch, Xbyak::CodeGenerator::T_NEAR);
                    }
                }
            }
        }
    }
    m_codegen.jmp(lblContinue);

    // One of the entries has a generation mismatch, which means code was potentially modified
    m_codegen.L(lblMismatch);
    {
        // Mark block as invalid by setting the host code pointer to null.
        // This will cause the recompiler to request a block invalidation later on, to clean up patches.
        const auto blockPtr = m_compiledCode.blockCache.Get(baseLoc.ToUint64());
        if (blockPtr != nullptr) {
            m_codegen.mov(basePtrReg64, CastUintPtr(blockPtr));
            m_codegen.mov(qword[basePtrReg64], CastUintPtr(nullptr));
        }

        // Go to epilog to recompile the block
        m_codegen.jmp(m_compiledCode.epilog);
    }

    // All checks passed, continue execution
    m_codegen.L(lblContinue);
    m_regAlloc.ReleaseTemporaries();
}

void x64Host::Compiler::CompileIRQLineCheck() {
    const auto irqLineOffset = m_stateOffsets.IRQLineOffset();
    auto tmpReg8 = GetReg8(m_regAlloc.GetTemporary());

    // Get inverted CPSR I bit
    m_codegen.test(abi::kHostFlagsReg, x64flgI);
    m_codegen.sete(tmpReg8);

    // Compare against IRQ line
    m_codegen.test(byte[abi::kARMStateReg + irqLineOffset], tmpReg8);

    // Jump to IRQ switch code if the IRQ line is raised and interrupts are not inhibited
    m_codegen.jnz(m_compiledCode.irqEntry);
    m_regAlloc.ReleaseTemporaries();
}

void x64Host::Compiler::CompileCondCheck(arm::Condition cond, Xbyak::Label &lblCondFail) {
    switch (cond) {
    case arm::Condition::EQ: // Z=1
        m_codegen.sahf();
        m_codegen.jnz(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::NE: // Z=0
        m_codegen.sahf();
        m_codegen.jz(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::CS: // C=1
        m_codegen.sahf();
        m_codegen.jnc(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::CC: // C=0
        m_codegen.sahf();
        m_codegen.jc(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::MI: // N=1
        m_codegen.sahf();
        m_codegen.jns(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::PL: // N=0
        m_codegen.sahf();
        m_codegen.js(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::VS: // V=1
        m_codegen.cmp(al, 0x81);
        m_codegen.jno(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::VC: // V=0
        m_codegen.cmp(al, 0x81);
        m_codegen.jo(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::HI: // C=1 && Z=0
        m_codegen.sahf();
        m_codegen.cmc();
        m_codegen.jna(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::LS: // C=0 || Z=1
        m_codegen.sahf();
        m_codegen.cmc();
        m_codegen.ja(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::GE: // N=V
        m_codegen.cmp(al, 0x81);
        m_codegen.sahf();
        m_codegen.jnge(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::LT: // N!=V
        m_codegen.cmp(al, 0x81);
        m_codegen.sahf();
        m_codegen.jge(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::GT: // Z=0 && N=V
        m_codegen.cmp(al, 0x81);
        m_codegen.sahf();
        m_codegen.jng(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::LE: // Z=1 || N!=V
        m_codegen.cmp(al, 0x81);
        m_codegen.sahf();
        m_codegen.jg(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::AL: // always
        break;
    case arm::Condition::NV: // never
        // not needed as the block code is not compiled
        // m_codegen.jmp(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    }
}

void x64Host::Compiler::CompileTerminal(const ir::BasicBlock &block) {
    if (!m_compiledCode.enableBlockLinking) {
        CompileExit();
        return;
    }

    using Terminal = ir::BasicBlock::Terminal;
    const auto blockLocKey = block.Location().ToUint64();

    switch (block.GetTerminal()) {
    case Terminal::DirectLink: {
        CompileDirectLink(block.GetTerminalLocation(), blockLocKey);
        break;
    }
    case Terminal::IndirectLink: {
        // Get current CPSR and PC
        const auto cpsrOffset = m_stateOffsets.CPSROffset();
        const auto pcRegOffset = m_stateOffsets.GPROffset(arm::GPR::PC, m_mode);

        // Build cache key
        auto cacheKeyReg64 = m_regAlloc.GetTemporary().cvt64();
        auto pcReg32 = m_regAlloc.GetTemporary();
        m_codegen.mov(cacheKeyReg64.cvt32(), dword[abi::kARMStateReg + cpsrOffset]);
        m_codegen.mov(pcReg32, dword[abi::kARMStateReg + pcRegOffset]);
        m_codegen.and_(cacheKeyReg64, 0x3F); // We only need the mode and T bits
        m_codegen.shl(cacheKeyReg64, 32);
        m_codegen.or_(cacheKeyReg64, pcReg32.cvt64());

        // Lookup entry
        auto tmpReg64 = m_regAlloc.GetTemporary().cvt64();
        auto jmpDstReg64 = pcReg32.cvt64();
        m_codegen.mov(jmpDstReg64, m_compiledCode.blockCache.MapAddress());

        using CacheType = decltype(m_compiledCode.blockCache);

        // Level 1 check
        m_codegen.mov(tmpReg64, cacheKeyReg64);
        m_codegen.shr(tmpReg64, CacheType::kL1Shift);
        // m_codegen.and_(tmpReg64, CacheType::kL1Mask); // shouldn't be necessary
        m_codegen.mov(jmpDstReg64, qword[jmpDstReg64 + tmpReg64 * sizeof(void *)]);
        m_codegen.test(jmpDstReg64, jmpDstReg64);
        m_codegen.jz(m_compiledCode.epilog);

        // Level 2 check
        m_codegen.mov(tmpReg64, cacheKeyReg64);
        m_codegen.shr(tmpReg64, CacheType::kL2Shift);
        m_codegen.and_(tmpReg64, CacheType::kL2Mask);
        m_codegen.mov(jmpDstReg64, qword[jmpDstReg64 + tmpReg64 * sizeof(void *)]);
        m_codegen.test(jmpDstReg64, jmpDstReg64);
        m_codegen.jz(m_compiledCode.epilog);

        // Level 3 check
        // m_codegen.shr(cacheKeyReg64, CacheType::kL3Shift); // shift by zero
        m_codegen.and_(cacheKeyReg64, CacheType::kL3Mask);
        static constexpr auto valueSize = CacheType::kValueSize;
        if constexpr (valueSize >= 1 && valueSize <= 8 && std::popcount(valueSize) == 1) {
            m_codegen.mov(jmpDstReg64, qword[jmpDstReg64 + cacheKeyReg64 * valueSize]);
        } else {
            m_codegen.imul(cacheKeyReg64, cacheKeyReg64, valueSize);
            m_codegen.mov(jmpDstReg64, qword[jmpDstReg64 + cacheKeyReg64]);
        }

        // Check for nullptr
        m_codegen.test(jmpDstReg64, jmpDstReg64);

        // Entry not found, jump to epilog
        m_codegen.jz(m_compiledCode.epilog);

        // Entry found, jump to linked block
        m_codegen.jmp(jmpDstReg64);
        m_regAlloc.ReleaseTemporaries();
        break;
    }
    case Terminal::Return: CompileExit(); break;
    }
}

void x64Host::Compiler::CompileDirectLinkToSuccessor(const ir::BasicBlock &block) {
    if (m_compiledCode.enableBlockLinking) {
        CompileDirectLink(block.NextLocation(), block.Location().ToUint64());
    } else {
        CompileExit();
    }
}

void x64Host::Compiler::CompileExit() {
    m_codegen.jmp(m_compiledCode.epilog, Xbyak::CodeGenerator::T_NEAR);
}

void x64Host::Compiler::ReserveTerminalRegisters(const ir::BasicBlock &block) {
    // This is invoked when the block condition is NV, which means CompileTerminal is only invoked after the condition
    // fail block, which may potentially branch to another block before the current block has the chance to spill
    // variables to the stack.
    if (!m_compiledCode.enableBlockLinking) {
        return;
    }

    // Indirect links use three temporary registers.
    // Reserve them now to force variable spilling before branches.
    if (block.GetTerminal() == ir::BasicBlock::Terminal::IndirectLink) {
        m_regAlloc.GetTemporary();
        m_regAlloc.GetTemporary();
        m_regAlloc.GetTemporary();
        m_regAlloc.ReleaseTemporaries();
    }
}

void x64Host::Compiler::CountCycles(uint64_t cycles) {
    if (cycles > 1) {
        if (m_armState.deadlinePtr != nullptr) {
            m_codegen.add(abi::kCycleCountReg, cycles);
        } else {
            m_codegen.sub(abi::kCycleCountReg, cycles);
        }
    } else if (cycles == 1) {
        if (m_armState.deadlinePtr != nullptr) {
            m_codegen.inc(abi::kCycleCountReg);
        } else {
            m_codegen.dec(abi::kCycleCountReg);
        }
    }
}

void x64Host::Compiler::CompileDirectLink(LocationRef target, uint64_t blockLocKey) {
    if (!m_compiledCode.enableBlockLinking) {
        CompileExit();
        return;
    }

    CompiledCode::PatchInfo patchInfo{.cachedBlockKey = blockLocKey, .codePos = m_codegen.getCurr()};
    patchInfo.codeEnd = m_codegen.getCurr();

    auto block = m_compiledCode.blockCache.Get(target.ToUint64());
    if (block != nullptr && *block != nullptr) {
        auto code = *block;

        // Jump to the compiled code's address directly
        m_codegen.jmp(code, Xbyak::CodeGenerator::T_NEAR);

        // Store this code location as "patched"
        m_compiledCode.appliedPatches.insert({target.ToUint64(), patchInfo});
    } else {
        // Exit due to cache miss; need to compile new block
        CompileExit();

        // Store this code location to be patched later
        m_compiledCode.pendingPatches.insert({target.ToUint64(), patchInfo});
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void x64Host::Compiler::CompileOp(const ir::IRGetRegisterOp *op) {
    auto dstReg32 = m_regAlloc.Get(op->dst.var);
    auto offset = m_stateOffsets.GPROffset(op->src.gpr, op->src.Mode());
    m_codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::Compiler::CompileOp(const ir::IRSetRegisterOp *op) {
    auto offset = m_stateOffsets.GPROffset(op->dst.gpr, op->dst.Mode());
    if (op->src.immediate) {
        m_codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);
    } else {
        auto srcReg32 = m_regAlloc.Get(op->src.var.var);
        m_codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRGetCPSROp *op) {
    auto dstReg32 = m_regAlloc.Get(op->dst.var);
    auto offset = m_stateOffsets.CPSROffset();
    m_codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::Compiler::CompileOp(const ir::IRSetCPSROp *op) {
    auto offset = m_stateOffsets.CPSROffset();
    if (op->src.immediate) {
        m_codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);

        // Update I in EAX
        if (op->updateIFlag) {
            if (bit::test<ARMflgIPos>(op->src.imm.value)) {
                m_codegen.or_(abi::kHostFlagsReg, x64flgI);
            } else {
                m_codegen.and_(abi::kHostFlagsReg, ~x64flgI);
            }
        }
    } else {
        auto srcReg32 = m_regAlloc.Get(op->src.var.var);
        m_codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);

        // Update I in EAX
        if (op->updateIFlag) {
            auto tmpReg32 = m_regAlloc.GetTemporary();
            m_codegen.mov(tmpReg32, srcReg32);
            m_codegen.and_(tmpReg32, ARMflgI);
            m_codegen.and_(abi::kHostFlagsReg, ~x64flgI);
            m_codegen.shl(tmpReg32, x64flgIPos - ARMflgIPos);
            m_codegen.or_(abi::kHostFlagsReg, tmpReg32);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRGetSPSROp *op) {
    auto dstReg32 = m_regAlloc.Get(op->dst.var);
    auto offset = m_stateOffsets.SPSROffset(op->mode);
    m_codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::Compiler::CompileOp(const ir::IRSetSPSROp *op) {
    auto offset = m_stateOffsets.SPSROffset(op->mode);
    if (op->src.immediate) {
        m_codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);
    } else {
        auto srcReg32 = m_regAlloc.Get(op->src.var.var);
        m_codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRMemReadOp *op) {
    // TODO: handle caches, permissions, etc.
    // TODO: virtual memory, exception handling, rewriting accessors

    Xbyak::Label lblEnd{};

    // Get destination register if present
    Xbyak::Reg32 dstReg32{};
    if (op->dst.var.IsPresent()) {
        dstReg32 = m_regAlloc.Get(op->dst.var);
    } else {
        dstReg32 = m_regAlloc.GetTemporary();
    }

    // Get base address register if it is a variable
    Xbyak::Reg32 baseAddrReg32{};
    if (!op->address.immediate) {
        baseAddrReg32 = m_regAlloc.Get(op->address.var.var);
    }

    // Reserve temporary registers for the slow memory path
    auto memMapReg64 = m_regAlloc.GetTemporary().cvt64();
    Xbyak::Reg32 indexReg32{};
    if (!op->address.immediate) {
        indexReg32 = m_regAlloc.GetTemporary();
    }

    auto compileRead = [this, op, &lblEnd, &baseAddrReg32](Xbyak::Reg32 dstReg32, Xbyak::Reg64 addrReg64, auto offset) {
        switch (op->size) {
        case ir::MemAccessSize::Byte:
            if (op->mode == ir::MemAccessMode::Signed) {
                m_codegen.movsx(dstReg32, byte[addrReg64 + offset]);
            } else { // aligned/unaligned
                m_codegen.movzx(dstReg32, byte[addrReg64 + offset]);
            }
            break;
        case ir::MemAccessSize::Half:
            if (op->mode == ir::MemAccessMode::Signed) {
                if (m_context.GetCPUArch() == CPUArch::ARMv4T) {
                    if (op->address.immediate) {
                        if (op->address.imm.value & 1) {
                            m_codegen.movsx(dstReg32, byte[addrReg64 + offset + 1]);
                        } else {
                            m_codegen.movsx(dstReg32, word[addrReg64 + offset]);
                        }
                    } else {
                        Xbyak::Label lblByteRead{};

                        m_codegen.test(baseAddrReg32, 1);
                        m_codegen.jnz(lblByteRead);

                        // Word read
                        m_codegen.movsx(dstReg32, word[addrReg64 + offset]);
                        m_codegen.jmp(lblEnd);

                        // Byte read
                        m_codegen.L(lblByteRead);
                        m_codegen.movsx(dstReg32, byte[addrReg64 + offset + 1]);
                    }
                } else {
                    m_codegen.movsx(dstReg32, word[addrReg64 + offset]);
                }
            } else if (op->mode == ir::MemAccessMode::Unaligned) {
                m_codegen.movzx(dstReg32, word[addrReg64 + offset]);
                if (m_context.GetCPUArch() == CPUArch::ARMv4T) {
                    if (op->address.immediate) {
                        const uint32_t shiftOffset = (op->address.imm.value & 1) * 8;
                        if (shiftOffset != 0) {
                            m_codegen.ror(dstReg32.cvt16(), shiftOffset);
                        }
                    } else {
                        auto shiftReg32 = m_regAlloc.GetRCX().cvt32();
                        m_codegen.mov(shiftReg32, baseAddrReg32);
                        m_codegen.and_(shiftReg32, 1);
                        m_codegen.shl(shiftReg32, 3);
                        m_codegen.ror(dstReg32.cvt16(), GetReg8(shiftReg32));
                    }
                }
            } else { // aligned
                m_codegen.movzx(dstReg32, word[addrReg64 + offset]);
            }
            break;
        case ir::MemAccessSize::Word:
            m_codegen.mov(dstReg32, dword[addrReg64 + offset]);
            if (op->mode == ir::MemAccessMode::Unaligned) {
                if (op->address.immediate) {
                    const uint32_t shiftOffset = (op->address.imm.value & 3) * 8;
                    if (shiftOffset != 0) {
                        m_codegen.ror(dstReg32, shiftOffset);
                    }
                } else {
                    auto shiftReg32 = m_regAlloc.GetRCX().cvt32();
                    m_codegen.mov(shiftReg32, baseAddrReg32);
                    m_codegen.and_(shiftReg32, 3);
                    m_codegen.shl(shiftReg32, 3);
                    m_codegen.ror(dstReg32, GetReg8(shiftReg32));
                }
            }
            break;
        }
    };

    Xbyak::Label lblSlowMem;

    const uint32_t addrMask = (op->size == ir::MemAccessSize::Word)   ? ~3
                              : (op->size == ir::MemAccessSize::Half) ? ~1
                                                                      : ~0;

    // Get memory map for the corresponding bus
    auto &memMapRef = (op->bus == ir::MemAccessBus::Code) ? m_memMap.codeRead : m_memMap.dataRead;

    // Get map pointer
    m_codegen.mov(memMapReg64, memMapRef.GetL1MapAddress());

    if (op->address.immediate) {
        const uint32_t address = op->address.imm.value;

        // Get level 1 pointer
        const uint32_t l1Index = address >> memMapRef.GetL1Shift();
        m_codegen.mov(memMapReg64, qword[memMapReg64 + l1Index * sizeof(void *)]);
        m_codegen.test(memMapReg64, memMapReg64);
        m_codegen.je(lblSlowMem);

        // Get level 2 pointer
        const uint32_t l2Index = (address >> memMapRef.GetL2Shift()) & memMapRef.GetL2Mask();
        m_codegen.mov(memMapReg64, qword[memMapReg64 + l2Index * sizeof(void *)]);
        m_codegen.test(memMapReg64, memMapReg64);
        m_codegen.je(lblSlowMem);

        // Read from selected page
        if (op->dst.var.IsPresent()) {
            const uint32_t offset = address & memMapRef.GetPageMask() & addrMask;
            compileRead(dstReg32, memMapReg64, offset);
        }
    } else {
        // Get level 1 pointer
        m_codegen.mov(indexReg32, baseAddrReg32);
        m_codegen.shr(indexReg32, memMapRef.GetL1Shift());
        m_codegen.mov(memMapReg64, qword[memMapReg64 + indexReg32.cvt64() * sizeof(void *)]);
        m_codegen.test(memMapReg64, memMapReg64);
        m_codegen.je(lblSlowMem);

        // Get level 2 pointer
        m_codegen.mov(indexReg32, baseAddrReg32);
        m_codegen.shr(indexReg32, memMapRef.GetL2Shift());
        m_codegen.and_(indexReg32, memMapRef.GetL2Mask());
        m_codegen.mov(memMapReg64, qword[memMapReg64 + indexReg32.cvt64() * sizeof(void *)]);
        m_codegen.test(memMapReg64, memMapReg64);
        m_codegen.je(lblSlowMem);

        // Read from selected page
        if (op->dst.var.IsPresent()) {
            m_codegen.mov(indexReg32, baseAddrReg32);
            m_codegen.and_(indexReg32, memMapRef.GetPageMask() & addrMask);
            compileRead(dstReg32, memMapReg64, indexReg32.cvt64());
        }
    }

    // Skip slow memory handler
    m_codegen.jmp(lblEnd);
    m_codegen.L(lblSlowMem);

    // Select parameters based on size
    // Valid combinations: aligned/signed byte, aligned/unaligned/signed half, aligned/unaligned word
    using ReadFn = uint32_t (*)(ISystem &system, uint32_t address);

    ReadFn readFn;
    switch (op->size) {
    case ir::MemAccessSize::Byte:
        if (op->mode == ir::MemAccessMode::Signed) {
            readFn = SystemMemReadSignedByte;
        } else { // aligned/unaligned
            readFn = SystemMemReadByte;
        }
        break;
    case ir::MemAccessSize::Half:
        if (op->mode == ir::MemAccessMode::Signed) {
            if (m_context.GetCPUArch() == CPUArch::ARMv4T) {
                readFn = SystemMemReadSignedHalfOrByte;
            } else {
                readFn = SystemMemReadSignedHalf;
            }
        } else if (op->mode == ir::MemAccessMode::Unaligned) {
            if (m_context.GetCPUArch() == CPUArch::ARMv4T) {
                readFn = SystemMemReadUnalignedRotatedHalf;
            } else {
                readFn = SystemMemReadUnalignedHalf;
            }
        } else { // aligned
            readFn = SystemMemReadAlignedHalf;
        }
        break;
    case ir::MemAccessSize::Word:
        if (op->mode == ir::MemAccessMode::Unaligned) {
            readFn = SystemMemReadUnalignedWord;
        } else { // aligned
            readFn = SystemMemReadAlignedWord;
        }
        break;
    default: util::unreachable();
    }

    auto &system = m_context.GetSystem();

    if (op->address.immediate) {
        CompileInvokeHostFunction(dstReg32, readFn, system, op->address.imm.value);
    } else {
        auto addrReg32 = m_regAlloc.Get(op->address.var.var);
        CompileInvokeHostFunction(dstReg32, readFn, system, addrReg32);
    }

    m_codegen.L(lblEnd);
}

void x64Host::Compiler::CompileOp(const ir::IRMemWriteOp *op) {
    // TODO: handle caches, permissions, etc.
    // TODO: virtual memory, exception handling, rewriting accessors

    // Get source register if it is a variable
    Xbyak::Reg32 srcReg32{};
    if (!op->src.immediate) {
        srcReg32 = m_regAlloc.Get(op->src.var.var);
    }

    // Get address register if it is a variable
    Xbyak::Reg32 addrReg32{};
    if (!op->address.immediate) {
        addrReg32 = m_regAlloc.Get(op->address.var.var);
    }

    // Reserve temporary register for the slow memory path
    Xbyak::Reg32 indexReg32{};
    if (!op->address.immediate) {
        indexReg32 = m_regAlloc.GetTemporary();
    }

    using MGT = MemoryGenerationTracker;
    auto &mgt = m_compiledCode.memGenTracker;

    // Increment memory page generation
    Xbyak::Label lblDone{};
    auto genReg64 = m_regAlloc.GetTemporary().cvt64();
    if (op->address.immediate) {
        const uint32_t address = op->address.imm.value;
        const uint32_t level = mgt.GetLevel(address);
        const uint32_t index1 = MGT::Level1Index(address);

        m_codegen.mov(genReg64, mgt.MapAddress() + index1 * sizeof(void *));
        if (level == 1) {
            m_codegen.cmp(byte[genReg64 + 7], MGT::kL1SplitThreshold);
            m_codegen.jae(lblDone);
            m_codegen.inc(byte[genReg64 + 7]);
        } else if (level == 2) {
            const uint32_t index2 = MGT::Level2Index(address);
            m_codegen.mov(rcx, ~0xFF000000'00000000);
            m_codegen.mov(genReg64, qword[genReg64]);
            m_codegen.and_(genReg64, rcx);
            m_codegen.cmp(byte[genReg64 + index2 * sizeof(void *) + 7], MGT::kL2SplitThreshold);
            m_codegen.jae(lblDone);
            m_codegen.inc(byte[genReg64 + index2 * sizeof(void *) + 7]);
        } else if (level == 3) {
            const uint32_t index2 = MGT::Level2Index(address);
            const uint32_t index3 = MGT::Level3Index(address);
            m_codegen.mov(rcx, ~0xFF000000'00000000);
            m_codegen.mov(genReg64, qword[genReg64]);
            m_codegen.and_(genReg64, rcx);
            m_codegen.mov(genReg64, qword[genReg64 + index2 * sizeof(void *)]);
            m_codegen.and_(genReg64, rcx);
            m_codegen.inc(dword[genReg64 + index3 * sizeof(uint32_t)]);
        }
    } else {
        auto tmpReg32 = m_regAlloc.GetTemporary();
        auto ptrReg64 = m_regAlloc.GetTemporary().cvt64();
        auto ptrReg8 = GetReg8(ptrReg64);
        auto maskReg64 = m_regAlloc.GetRCX();

        Xbyak::Label lblNoInc2{};
        Xbyak::Label lblLevel2{};
        Xbyak::Label lblLevel3{};

        m_codegen.mov(genReg64, mgt.MapAddress());

        // Level 1
        m_codegen.mov(tmpReg32, addrReg32);
        m_codegen.shr(tmpReg32, MGT::kL1Shift);
        // m_codegen.and_(tmpReg32, MGT::kL1Mask); // shouldn't be necessary
        m_codegen.lea(genReg64, qword[genReg64 + tmpReg32.cvt64() * sizeof(void *)]);
        m_codegen.mov(ptrReg64, qword[genReg64]);

        m_codegen.rol(ptrReg64, 8);
        m_codegen.cmp(ptrReg8, 0xFF);
        m_codegen.je(lblLevel2);

        m_codegen.cmp(byte[genReg64 + 7], MGT::kL1SplitThreshold);
        m_codegen.jae(lblDone);
        m_codegen.inc(byte[genReg64 + 7]);
        m_codegen.jmp(lblDone);

        // Level 2
        m_codegen.L(lblLevel2);
        m_codegen.ror(ptrReg64, 8);
        m_codegen.mov(maskReg64, ~0xFF000000'00000000);
        m_codegen.and_(ptrReg64, maskReg64);

        m_codegen.mov(tmpReg32, addrReg32);
        m_codegen.shr(tmpReg32, MGT::kL2Shift);
        m_codegen.and_(tmpReg32, MGT::kL2Mask);
        m_codegen.lea(genReg64, qword[ptrReg64 + tmpReg32.cvt64() * sizeof(void *)]);
        m_codegen.mov(ptrReg64, qword[genReg64]);

        m_codegen.rol(ptrReg64, 8);
        m_codegen.cmp(ptrReg8, 0xFF);
        m_codegen.je(lblLevel3);

        m_codegen.cmp(byte[genReg64 + 7], MGT::kL2SplitThreshold);
        m_codegen.jae(lblDone);
        m_codegen.inc(byte[genReg64 + 7]);
        m_codegen.jmp(lblDone);

        // Level 3
        m_codegen.L(lblLevel3);
        m_codegen.ror(ptrReg64, 8);
        m_codegen.and_(ptrReg64, maskReg64);

        m_codegen.mov(tmpReg32, addrReg32);
        m_codegen.shr(tmpReg32, MGT::kL3Shift);
        m_codegen.and_(tmpReg32, MGT::kL3Mask);

        m_codegen.inc(dword[ptrReg64 + tmpReg32.cvt64() * sizeof(uint32_t)]);
    }
    m_codegen.L(lblDone);

    Xbyak::Label lblSlowMem;
    Xbyak::Label lblEnd{};

    const uint32_t addrMask = (op->size == ir::MemAccessSize::Word)   ? ~3
                              : (op->size == ir::MemAccessSize::Half) ? ~1
                                                                      : ~0;
    // Get memory map for the corresponding bus
    auto &memMapRef = m_memMap.dataWrite;

    // Get map pointer
    auto memMapReg64 = genReg64; // Reuse generation register
    m_codegen.mov(memMapReg64, memMapRef.GetL1MapAddress());

    if (op->address.immediate) {
        const uint32_t address = op->address.imm.value;

        // Get level 1 pointer
        const uint32_t l1Index = address >> memMapRef.GetL1Shift();
        m_codegen.mov(memMapReg64, qword[memMapReg64 + l1Index * sizeof(void *)]);
        m_codegen.test(memMapReg64, memMapReg64);
        m_codegen.je(lblSlowMem);

        // Get level 2 pointer
        const uint32_t l2Index = (address >> memMapRef.GetL2Shift()) & memMapRef.GetL2Mask();
        m_codegen.mov(memMapReg64, qword[memMapReg64 + l2Index * sizeof(void *)]);
        m_codegen.test(memMapReg64, memMapReg64);
        m_codegen.je(lblSlowMem);

        // Write to selected page
        uint32_t offset = address & memMapRef.GetPageMask() & addrMask;
        if (op->src.immediate) {
            const uint32_t imm = op->src.imm.value;
            switch (op->size) {
            case ir::MemAccessSize::Byte: m_codegen.mov(byte[memMapReg64 + offset], (uint8_t)imm); break;
            case ir::MemAccessSize::Half: m_codegen.mov(word[memMapReg64 + offset], (uint16_t)imm); break;
            case ir::MemAccessSize::Word: m_codegen.mov(dword[memMapReg64 + offset], imm); break;
            default: util::unreachable();
            }
        } else {
            switch (op->size) {
            case ir::MemAccessSize::Byte: m_codegen.mov(byte[memMapReg64 + offset], GetReg8(srcReg32)); break;
            case ir::MemAccessSize::Half: m_codegen.mov(word[memMapReg64 + offset], srcReg32.cvt16()); break;
            case ir::MemAccessSize::Word: m_codegen.mov(dword[memMapReg64 + offset], srcReg32); break;
            default: util::unreachable();
            }
        }
    } else {
        // Get level 1 pointer
        m_codegen.mov(indexReg32, addrReg32);
        m_codegen.shr(indexReg32, memMapRef.GetL1Shift());
        m_codegen.mov(memMapReg64, qword[memMapReg64 + indexReg32.cvt64() * sizeof(void *)]);
        m_codegen.test(memMapReg64, memMapReg64);
        m_codegen.je(lblSlowMem);

        // Get level 2 pointer
        m_codegen.mov(indexReg32, addrReg32);
        m_codegen.shr(indexReg32, memMapRef.GetL2Shift());
        m_codegen.and_(indexReg32, memMapRef.GetL2Mask());
        m_codegen.mov(memMapReg64, qword[memMapReg64 + indexReg32.cvt64() * sizeof(void *)]);
        m_codegen.test(memMapReg64, memMapReg64);
        m_codegen.je(lblSlowMem);

        // Write to selected page
        m_codegen.mov(indexReg32, addrReg32);
        m_codegen.and_(indexReg32, memMapRef.GetPageMask() & addrMask);
        if (op->src.immediate) {
            const uint32_t imm = op->src.imm.value;
            switch (op->size) {
            case ir::MemAccessSize::Byte: m_codegen.mov(byte[memMapReg64 + indexReg32.cvt64()], (uint8_t)imm); break;
            case ir::MemAccessSize::Half: m_codegen.mov(word[memMapReg64 + indexReg32.cvt64()], (uint16_t)imm); break;
            case ir::MemAccessSize::Word: m_codegen.mov(dword[memMapReg64 + indexReg32.cvt64()], imm); break;
            default: util::unreachable();
            }
        } else {
            switch (op->size) {
            case ir::MemAccessSize::Byte:
                m_codegen.mov(byte[memMapReg64 + indexReg32.cvt64()], GetReg8(srcReg32));
                break;
            case ir::MemAccessSize::Half:
                m_codegen.mov(word[memMapReg64 + indexReg32.cvt64()], srcReg32.cvt16());
                break;
            case ir::MemAccessSize::Word: m_codegen.mov(dword[memMapReg64 + indexReg32.cvt64()], srcReg32); break;
            default: util::unreachable();
            }
        }
    }

    // Skip slow memory handler
    m_codegen.jmp(lblEnd);

    // Handle slow memory access
    m_codegen.L(lblSlowMem);

    auto &system = m_context.GetSystem();

    auto invokeFnImm8 = [&](auto fn, const ir::VarOrImmArg &address, uint8_t src) {
        if (address.immediate) {
            CompileInvokeHostFunction(fn, system, address.imm.value, (uint32_t)src);
        } else {
            CompileInvokeHostFunction(fn, system, addrReg32, (uint32_t)src);
        }
    };

    auto invokeFnImm16 = [&](auto fn, const ir::VarOrImmArg &address, uint16_t src) {
        if (address.immediate) {
            CompileInvokeHostFunction(fn, system, address.imm.value, (uint32_t)src);
        } else {
            CompileInvokeHostFunction(fn, system, addrReg32, (uint32_t)src);
        }
    };

    auto invokeFnImm32 = [&](auto fn, const ir::VarOrImmArg &address, uint32_t src) {
        if (address.immediate) {
            CompileInvokeHostFunction(fn, system, address.imm.value, src);
        } else {
            CompileInvokeHostFunction(fn, system, addrReg32, src);
        }
    };

    auto invokeFnReg32 = [&](auto fn, const ir::VarOrImmArg &address, ir::Variable src) {
        if (address.immediate) {
            CompileInvokeHostFunction(fn, system, address.imm.value, srcReg32);
        } else {
            CompileInvokeHostFunction(fn, system, addrReg32, srcReg32);
        }
    };

    auto invokeFn = [&](auto valueFn, auto fn) {
        if (op->src.immediate) {
            valueFn(fn, op->address, op->src.imm.value);
        } else {
            invokeFnReg32(fn, op->address, op->src.var.var);
        }
    };

    // Invoke appropriate write function
    switch (op->size) {
    case ir::MemAccessSize::Byte: invokeFn(invokeFnImm8, SystemMemWriteByte); break;
    case ir::MemAccessSize::Half: invokeFn(invokeFnImm16, SystemMemWriteHalf); break;
    case ir::MemAccessSize::Word: invokeFn(invokeFnImm32, SystemMemWriteWord); break;
    default: util::unreachable();
    }

    m_codegen.L(lblEnd);
}

void x64Host::Compiler::CompileOp(const ir::IRPreloadOp *op) {
    // TODO: implement
}

void x64Host::Compiler::CompileOp(const ir::IRLogicalShiftLeftOp *op) {
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // x86 masks the shift amount to 31 or 63.
    // ARM does not -- larger amounts simply output zero.
    // For offset == 32, the carry flag is set to bit 0 of the base value.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::LSL(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg64 = m_regAlloc.GetRCX();
        auto valueReg64 = m_regAlloc.Get(op->value.var.var).cvt64();
        auto amountReg32 = m_regAlloc.Get(op->amount.var.var);
        auto amountReg8 = GetReg8(amountReg32);

        // Get shift amount, clamped to 0..63
        m_codegen.mov(shiftReg64, 63);
        m_codegen.cmp(amountReg8, 63);
        m_codegen.cmovbe(shiftReg64, amountReg32.cvt64());
        m_codegen.movzx(shiftReg64, GetReg8(shiftReg64));

        // Get destination register
        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            auto dstReg64 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            m_codegen.shlx(dstReg64, valueReg64, shiftReg64);
        } else {
            Xbyak::Reg64 dstReg64{};
            if (op->dst.var.IsPresent()) {
                dstReg64 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
                CopyIfDifferent(dstReg64, valueReg64);
            } else {
                dstReg64 = m_regAlloc.GetTemporary().cvt64();
                m_codegen.mov(dstReg64, valueReg64);
            }

            // Compute the shift
            m_codegen.shl(dstReg64, 32);                  // Shift value to the top half of the 64-bit register
            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.shl(dstReg64, GetReg8(shiftReg64));
            if (op->setCarry) {
                SetCFromFlags();
            }
            if (op->dst.var.IsPresent()) {
                m_codegen.shr(dstReg64, 32); // Shift value back down to the bottom half
            }
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg64 = m_regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg32 = m_regAlloc.Get(op->amount.var.var);
        auto amountReg8 = GetReg8(amountReg32);

        // Get shift amount, clamped to 0..63
        m_codegen.mov(shiftReg64, 63);
        m_codegen.cmp(amountReg8, 63);
        m_codegen.cmovbe(shiftReg64, amountReg32.cvt64());
        m_codegen.movzx(shiftReg64, GetReg8(shiftReg64));

        // Get destination register
        Xbyak::Reg64 dstReg64{};
        if (op->dst.var.IsPresent()) {
            dstReg64 = m_regAlloc.Get(op->dst.var).cvt64();
        } else {
            dstReg64 = m_regAlloc.GetTemporary().cvt64();
        }

        // Compute the shift
        m_codegen.mov(dstReg64, static_cast<uint64_t>(value) << 32ull);
        m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
        m_codegen.shl(dstReg64, GetReg8(shiftReg64));
        if (op->setCarry) {
            SetCFromFlags();
        }
        if (op->dst.var.IsPresent()) {
            m_codegen.shr(dstReg64, 32); // Shift value back down to the bottom half
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = m_regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value & 0xFF;

        if (amount < 32) {
            // Get destination register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
            } else {
                dstReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(dstReg32, valueReg32);
            }

            // Compute shift and update flags
            m_codegen.shl(dstReg32, amount);
            if (amount > 0 && op->setCarry) {
                SetCFromFlags();
            }
        } else if (amount == 32) {
            if (op->dst.var.IsPresent()) {
                // Update carry flag before zeroing out the register
                if (op->setCarry) {
                    m_codegen.bt(valueReg32, 0);
                    SetCFromFlags();
                }

                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                m_codegen.xor_(dstReg32, dstReg32);
            } else if (op->setCarry) {
                m_codegen.bt(valueReg32, 0);
                SetCFromFlags();
            }
        } else {
            // Zero out destination
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                m_codegen.xor_(dstReg32, dstReg32);
            }
            if (op->setCarry) {
                SetCFromValue(false);
            }
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRLogicalShiftRightOp *op) {
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // x86 masks the shift amount to 31 or 63.
    // ARM does not -- larger amounts simply output zero.
    // For offset == 32, the carry flag is set to bit 31.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::LSR(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg64 = m_regAlloc.GetRCX();
        auto valueReg64 = m_regAlloc.Get(op->value.var.var).cvt64();
        auto amountReg32 = m_regAlloc.Get(op->amount.var.var);
        auto amountReg8 = GetReg8(amountReg32);

        // Get shift amount, clamped to 0..63
        m_codegen.mov(shiftReg64, 63);
        m_codegen.cmp(amountReg8, 63);
        m_codegen.cmovbe(shiftReg64, amountReg32.cvt64());
        m_codegen.movzx(shiftReg64, GetReg8(shiftReg64));

        // Compute the shift
        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            auto dstReg64 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            m_codegen.shrx(dstReg64, valueReg64, shiftReg64);
            m_codegen.mov(dstReg64.cvt32(), dstReg64.cvt32()); // TODO: ideally MOV to another register
        } else if (op->dst.var.IsPresent()) {
            auto dstReg64 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            CopyIfDifferent(dstReg64, valueReg64);
            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.shr(dstReg64, GetReg8(shiftReg64));
            if (op->setCarry) {
                SetCFromFlags();
            }
            m_codegen.mov(dstReg64.cvt32(), dstReg64.cvt32()); // TODO: ideally MOV to another register
        } else if (op->setCarry) {
            Xbyak::Label lblNoEffect{};
            m_codegen.cmp(shiftReg64, shiftReg64);
            m_codegen.jz(lblNoEffect);

            m_codegen.dec(shiftReg64);
            m_codegen.bt(valueReg64, shiftReg64);
            SetCFromFlags();

            m_codegen.L(lblNoEffect);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg64 = m_regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg32 = m_regAlloc.Get(op->amount.var.var);
        auto amountReg8 = GetReg8(amountReg32);

        // Get shift amount, clamped to 0..63
        m_codegen.mov(shiftReg64, 63);
        m_codegen.cmp(amountReg8, 63);
        m_codegen.cmovbe(shiftReg64, amountReg32.cvt64());
        m_codegen.movzx(shiftReg64, GetReg8(shiftReg64));

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = m_regAlloc.Get(op->dst.var).cvt64();
            m_codegen.mov(dstReg64, value);
            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.shr(dstReg64, GetReg8(shiftReg64));
            if (op->setCarry) {
                SetCFromFlags();
            }
            m_codegen.mov(dstReg64.cvt32(), dstReg64.cvt32()); // TODO: ideally MOV to another register
        } else if (op->setCarry) {
            auto valueReg64 = m_regAlloc.GetTemporary().cvt64();

            Xbyak::Label lblNoEffect{};
            m_codegen.cmp(shiftReg64, shiftReg64);
            m_codegen.jz(lblNoEffect);

            m_codegen.mov(valueReg64, (static_cast<uint64_t>(value) << 1ull));
            m_codegen.bt(valueReg64, shiftReg64);
            SetCFromFlags();

            m_codegen.L(lblNoEffect);
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = m_regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value & 0xFF;

        if (amount < 32) {
            // Compute the shift
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
                m_codegen.shr(dstReg32, amount);
                if (amount > 0 && op->setCarry) {
                    SetCFromFlags();
                }
            } else if (amount > 0 && op->setCarry) {
                m_codegen.bt(valueReg32.cvt64(), amount - 1);
                SetCFromFlags();
            }
        } else if (amount == 32) {
            if (op->dst.var.IsPresent()) {
                // Update carry flag before zeroing out the register
                if (op->setCarry) {
                    m_codegen.bt(valueReg32, 31);
                    SetCFromFlags();
                }

                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                m_codegen.xor_(dstReg32, dstReg32);
            } else if (op->setCarry) {
                m_codegen.bt(valueReg32, 31);
                SetCFromFlags();
            }
        } else {
            // Zero out destination
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                m_codegen.xor_(dstReg32, dstReg32);
            }
            if (op->setCarry) {
                SetCFromValue(false);
            }
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRArithmeticShiftRightOp *op) {
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // x86 masks the shift amount to 31 or 63.
    // ARM does not, though the output value is the same for shifts by 31 or more.
    // For offset == 32, the carry flag is set to bit 31.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::ASR(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg32 = m_regAlloc.GetRCX().cvt32();
        auto valueReg32 = m_regAlloc.Get(op->value.var.var);
        auto amountReg32 = m_regAlloc.Get(op->amount.var.var);
        auto amountReg8 = GetReg8(amountReg32);

        // Get shift amount, clamped to 0..32
        m_codegen.mov(shiftReg32, 32);
        m_codegen.cmp(amountReg8, 32);
        m_codegen.cmovbe(shiftReg32, amountReg32);
        m_codegen.movzx(shiftReg32, GetReg8(shiftReg32));

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            CopyIfDifferent(dstReg64.cvt32(), valueReg32);
            m_codegen.movsxd(dstReg64, dstReg64.cvt32());
            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.sar(dstReg64, GetReg8(shiftReg32));
            if (op->setCarry) {
                SetCFromFlags();
            }
            m_codegen.mov(dstReg64.cvt32(), dstReg64.cvt32()); // TODO: ideally MOV to another register
        } else if (op->setCarry) {
            Xbyak::Label lblNoEffect{};
            m_codegen.cmp(shiftReg32, shiftReg32);
            m_codegen.jz(lblNoEffect);

            m_codegen.dec(shiftReg32);
            m_codegen.bt(valueReg32, shiftReg32);
            SetCFromFlags();

            m_codegen.L(lblNoEffect);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg32 = m_regAlloc.GetRCX().cvt32();
        auto value = static_cast<int32_t>(op->value.imm.value);
        auto amountReg32 = m_regAlloc.Get(op->amount.var.var);
        auto amountReg8 = GetReg8(amountReg32);

        // Get shift amount, clamped to 0..32
        m_codegen.mov(shiftReg32, 32);
        m_codegen.cmp(amountReg8, 32);
        m_codegen.cmovbe(shiftReg32, amountReg32);
        m_codegen.movzx(shiftReg32, GetReg8(shiftReg32));

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = m_regAlloc.Get(op->dst.var).cvt64();
            m_codegen.mov(dstReg64, value);
            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.sar(dstReg64, GetReg8(shiftReg32));
            if (op->setCarry) {
                SetCFromFlags();
            }
            m_codegen.mov(dstReg64.cvt32(), dstReg64.cvt32()); // TODO: ideally MOV to another register
        } else if (op->setCarry) {
            auto valueReg64 = m_regAlloc.GetTemporary().cvt64();

            Xbyak::Label lblNoEffect{};
            m_codegen.cmp(shiftReg32, shiftReg32);
            m_codegen.jz(lblNoEffect);

            m_codegen.mov(valueReg64, (static_cast<uint64_t>(value) << 1ull));
            m_codegen.bt(valueReg64, shiftReg32.cvt64());
            SetCFromFlags();

            m_codegen.L(lblNoEffect);
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = m_regAlloc.Get(op->value.var.var);
        auto amount = std::min(op->amount.imm.value & 0xFF, 32u);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            m_codegen.movsxd(dstReg64, valueReg32);
            m_codegen.sar(dstReg64, amount);
            if (amount > 0 && op->setCarry) {
                SetCFromFlags();
            }
            m_codegen.mov(dstReg64.cvt32(), dstReg64.cvt32()); // TODO: ideally MOV to another register
        } else if (op->setCarry) {
            m_codegen.bt(valueReg32.cvt64(), amount - 1);
            SetCFromFlags();
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRRotateRightOp *op) {
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // x86 masks the shift amount to 31 or 63.
    // ARM does not, though the output value is the same for shifts by 32 or more.
    // For offsets that are positive multiples of 32, the carry flag is set to bit 31.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::ROR(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg32 = m_regAlloc.GetRCX().cvt32();
        auto valueReg32 = m_regAlloc.Get(op->value.var.var);
        auto amountReg32 = m_regAlloc.Get(op->amount.var.var);

        Xbyak::Reg32 dstReg32{};
        if (op->dst.var.IsPresent()) {
            dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
        } else {
            dstReg32 = m_regAlloc.GetTemporary();
        }

        // Skip if rotation amount is zero
        Xbyak::Label lblNoRotation{};
        m_codegen.test(amountReg32, amountReg32);
        m_codegen.jz(lblNoRotation);

        {
            // Put shift amount into ECX
            m_codegen.mov(shiftReg32, amountReg32);

            // Put value to shift into the result register
            if (op->dst.var.IsPresent()) {
                CopyIfDifferent(dstReg32, valueReg32);
            } else {
                m_codegen.mov(dstReg32, valueReg32);
            }

            // Compute the shift
            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.ror(dstReg32, GetReg8(shiftReg32));
            if (op->setCarry) {
                m_codegen.bt(dstReg32, 31);
                SetCFromFlags();
            }
        }

        m_codegen.L(lblNoRotation);
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg32 = m_regAlloc.GetRCX().cvt32();
        auto value = op->value.imm.value;
        auto amountReg32 = m_regAlloc.Get(op->amount.var.var);

        // Put value to shift into the result register
        Xbyak::Reg32 dstReg32{};
        if (op->dst.var.IsPresent()) {
            dstReg32 = m_regAlloc.Get(op->dst.var);
        } else {
            dstReg32 = m_regAlloc.GetTemporary();
        }

        // Skip if rotation amount is zero
        Xbyak::Label lblNoRotation{};
        m_codegen.test(amountReg32, amountReg32);
        m_codegen.jz(lblNoRotation);

        {
            // Put shift amount into ECX
            m_codegen.mov(shiftReg32, amountReg32);

            // Compute the shift
            m_codegen.mov(dstReg32, value);
            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.ror(dstReg32, GetReg8(shiftReg32));
            if (op->setCarry) {
                m_codegen.bt(dstReg32, 31);
                SetCFromFlags();
            }
        }

        m_codegen.L(lblNoRotation);
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = m_regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value & 0xFF;

        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            // Compute the shift directly into the result register
            auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            m_codegen.rorx(dstReg32, valueReg32, amount);
        } else {
            // Put value to shift into the result register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
            } else {
                dstReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(dstReg32, valueReg32);
            }

            // Compute the shift
            m_codegen.ror(dstReg32, amount);
            if (amount > 0 && op->setCarry) {
                // If rotating by a positive multiple of 32, set the carry to the MSB
                if ((amount & 31) == 0) {
                    m_codegen.bt(dstReg32, 31);
                }
                SetCFromFlags();
            }
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRRotateRightExtendedOp *op) {
    // ARM RRX works exactly the same as x86 RCR by 1, including carry flag behavior.

    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 dstReg32{};

        if (op->value.immediate) {
            dstReg32 = m_regAlloc.Get(op->dst.var);
            m_codegen.mov(dstReg32, op->value.imm.value);
        } else {
            auto valueReg32 = m_regAlloc.Get(op->value.var.var);
            dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valueReg32);
        }

        m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Refresh carry flag
        m_codegen.rcr(dstReg32, 1);                   // Perform RRX

        if (op->setCarry) {
            SetCFromFlags();
        }
    } else if (op->setCarry) {
        if (op->value.immediate) {
            SetCFromValue(bit::test<0>(op->value.imm.value));
        } else {
            auto valueReg32 = m_regAlloc.Get(op->value.var.var);
            m_codegen.bt(valueReg32, 0);
            SetCFromFlags();
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRBitwiseAndOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value & op->rhs.imm.value;
        AssignImmResultWithNZ(op->dst, result, op->flags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = m_regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.and_(dstReg32, imm);
            } else if (setFlags) {
                m_codegen.test(varReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                m_regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                m_regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = m_regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    m_codegen.and_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    m_codegen.and_(dstReg32, lhsReg32);
                } else {
                    m_codegen.mov(dstReg32, lhsReg32);
                    m_codegen.and_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                m_codegen.test(lhsReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZFromFlags(op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRBitwiseOrOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value | op->rhs.imm.value;
        AssignImmResultWithNZ(op->dst, result, op->flags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = m_regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.or_(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.or_(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                m_regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                m_regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = m_regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    m_codegen.or_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    m_codegen.or_(dstReg32, lhsReg32);
                } else {
                    m_codegen.mov(dstReg32, lhsReg32);
                    m_codegen.or_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();

                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.or_(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZFromFlags(op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRBitwiseXorOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value ^ op->rhs.imm.value;
        AssignImmResultWithNZ(op->dst, result, op->flags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = m_regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.xor_(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.xor_(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                m_regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                m_regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = m_regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    m_codegen.xor_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    m_codegen.xor_(dstReg32, lhsReg32);
                } else {
                    m_codegen.mov(dstReg32, lhsReg32);
                    m_codegen.xor_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();

                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.xor_(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZFromFlags(op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRBitClearOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value & ~op->rhs.imm.value;
        AssignImmResultWithNZ(op->dst, result, op->flags);
    } else {
        // At least one of the operands is a variable
        if (!rhsImm) {
            // lhs is var or imm, rhs is variable
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->rhs.var.var);
                CopyIfDifferent(dstReg32, rhsReg32);

                if (lhsImm) {
                    m_codegen.not_(dstReg32);
                    m_codegen.and_(dstReg32, op->lhs.imm.value);
                } else {
                    auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
                    if (CPUID::HasBMI1()) {
                        m_codegen.andn(dstReg32, dstReg32, lhsReg32);
                    } else {
                        m_codegen.not_(dstReg32);
                        m_codegen.and_(dstReg32, lhsReg32);
                    }
                }
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, rhsReg32);
                m_codegen.not_(tmpReg32);

                if (lhsImm) {
                    m_codegen.test(tmpReg32, op->lhs.imm.value);
                } else {
                    auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
                    m_codegen.test(tmpReg32, lhsReg32);
                }
            }
        } else {
            // lhs is variable, rhs is immediate
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(dstReg32, lhsReg32);
                m_codegen.and_(dstReg32, ~op->rhs.imm.value);
            } else if (setFlags) {
                m_codegen.test(lhsReg32, ~op->rhs.imm.value);
            }
        }

        if (setFlags) {
            SetNZFromFlags(op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRCountLeadingZerosOp *op) {
    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 valReg32{};
        Xbyak::Reg32 dstReg32{};
        if (op->value.immediate) {
            valReg32 = m_regAlloc.GetTemporary();
            dstReg32 = m_regAlloc.Get(op->dst.var);
            m_codegen.mov(valReg32, op->value.imm.value);
        } else {
            valReg32 = m_regAlloc.Get(op->value.var.var);
            dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valReg32);
        }

        if (CPUID::HasLZCNT()) {
            m_codegen.lzcnt(dstReg32, valReg32);
        } else {
            // BSR unhelpfully returns the bit offset from the right, not left
            auto valIfZero32 = m_regAlloc.GetTemporary();
            m_codegen.mov(valIfZero32, 0xFFFFFFFF);
            m_codegen.bsr(dstReg32, valReg32);
            m_codegen.cmovz(dstReg32, valIfZero32);
            m_codegen.neg(dstReg32);
            m_codegen.add(dstReg32, 31);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRAddOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        auto [result, carry, overflow] = arm::ADD(op->lhs.imm.value, op->rhs.imm.value);
        AssignImmResultWithNZCV(op->dst, result, carry, overflow, op->flags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = m_regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.add(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.add(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                m_regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                m_regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = m_regAlloc.Get(op->dst.var);

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    m_codegen.mov(dstReg32, lhsReg32);
                }
                m_codegen.add(dstReg32, op2Reg32);
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();

                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.add(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags(op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRAddCarryOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = m_regAlloc.Get(op->dst.var);
            m_codegen.mov(dstReg32, op->lhs.imm.value);

            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.adc(dstReg32, op->rhs.imm.value);
            if (setFlags) {
                SetNZCVFromFlags(op->flags);
            }
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = m_regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                m_codegen.adc(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                m_codegen.adc(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                m_regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                m_regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = m_regAlloc.Get(op->dst.var);

                m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    m_codegen.mov(dstReg32, lhsReg32);
                }
                m_codegen.adc(dstReg32, op2Reg32);
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();

                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                m_codegen.adc(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags(op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRSubtractOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        auto [result, carry, overflow] = arm::SUB(op->lhs.imm.value, op->rhs.imm.value);
        AssignImmResultWithNZCV(op->dst, result, carry, overflow, op->flags);
    } else {
        // At least one of the operands is a variable
        if (!lhsImm) {
            // lhs is variable, rhs is var or imm
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);

            if (op->dst.var.IsPresent()) {
                if (rhsImm) {
                    auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                    CopyIfDifferent(dstReg32, lhsReg32);
                    m_codegen.sub(dstReg32, op->rhs.imm.value);
                } else {
                    auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);
                    auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                    CopyIfDifferent(dstReg32, lhsReg32);
                    m_codegen.sub(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                if (rhsImm) {
                    m_codegen.cmp(lhsReg32, op->rhs.imm.value);
                } else {
                    auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);
                    m_codegen.cmp(lhsReg32, rhsReg32);
                }
            }
        } else {
            // lhs is immediate, rhs is variable
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.Get(op->dst.var);
                m_codegen.mov(dstReg32, op->lhs.imm.value);
                m_codegen.sub(dstReg32, rhsReg32);
            } else if (setFlags) {
                auto lhsReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(lhsReg32, op->lhs.imm.value);
                m_codegen.cmp(lhsReg32, rhsReg32);
            }
        }

        if (setFlags) {
            m_codegen.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
            SetNZCVFromFlags(op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRSubtractCarryOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    // Note: x86 and ARM have inverted borrow bits

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = m_regAlloc.Get(op->dst.var);
            m_codegen.mov(dstReg32, op->lhs.imm.value);

            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.cmc();                              // Complement it
            m_codegen.sbb(dstReg32, op->rhs.imm.value);
            if (setFlags) {
                m_codegen.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
                SetNZCVFromFlags(op->flags);
            }
        }
    } else {
        // At least one of the operands is a variable
        if (!lhsImm) {
            // lhs is variable, rhs is var or imm
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);

            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.cmc();                              // Complement it
            if (rhsImm) {
                Xbyak::Reg32 dstReg32{};
                if (op->dst.var.IsPresent()) {
                    dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                    CopyIfDifferent(dstReg32, lhsReg32);
                } else {
                    dstReg32 = m_regAlloc.GetTemporary();
                }
                m_codegen.sbb(dstReg32, op->rhs.imm.value);
            } else {
                auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);
                Xbyak::Reg32 dstReg32{};
                if (op->dst.var.IsPresent()) {
                    dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                    CopyIfDifferent(dstReg32, lhsReg32);
                } else {
                    dstReg32 = m_regAlloc.GetTemporary();
                }
                m_codegen.sbb(dstReg32, rhsReg32);
            }
        } else {
            // lhs is immediate, rhs is variable
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = m_regAlloc.Get(op->dst.var);
            } else {
                dstReg32 = m_regAlloc.GetTemporary();
            }
            m_codegen.mov(dstReg32, op->lhs.imm.value);
            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.cmc();                              // Complement it
            m_codegen.sbb(dstReg32, rhsReg32);
        }

        if (setFlags) {
            m_codegen.cmc(); // Complement carry output
            SetNZCVFromFlags(op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRMoveOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = m_regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg32, op->value.imm.value);
        } else {
            auto valReg32 = m_regAlloc.Get(op->value.var.var);
            auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valReg32);
        }

        if (setFlags) {
            auto dstReg32 = m_regAlloc.Get(op->dst.var);
            SetNZFromReg(dstReg32, op->flags);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(op->value.imm.value, op->flags);
        } else {
            auto valReg32 = m_regAlloc.Get(op->value.var.var);
            SetNZFromReg(valReg32, op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRMoveNegatedOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = m_regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg32, ~op->value.imm.value);
        } else {
            auto valReg32 = m_regAlloc.Get(op->value.var.var);
            auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);

            CopyIfDifferent(dstReg32, valReg32);
            m_codegen.not_(dstReg32);
        }

        if (setFlags) {
            auto dstReg32 = m_regAlloc.Get(op->dst.var);
            SetNZFromReg(dstReg32, op->flags);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(~op->value.imm.value, op->flags);
        } else {
            auto valReg32 = m_regAlloc.Get(op->value.var.var);
            auto tmpReg32 = m_regAlloc.GetTemporary();
            m_codegen.mov(tmpReg32, valReg32);
            m_codegen.not_(tmpReg32);
            SetNZFromReg(tmpReg32, op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRSignExtendHalfOp *op) {
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = m_regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg32, bit::sign_extend<16, int32_t>(op->value.imm.value));
        } else {
            auto valReg32 = m_regAlloc.Get(op->value.var.var);
            auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            m_codegen.movsx(dstReg32, valReg32.cvt16());
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRSaturatingAddOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).AnyOf(arm::Flags::V);

    if (lhsImm && rhsImm) {
        // Both are immediates
        const int64_t lhsVal = bit::sign_extend<32, int64_t>(op->lhs.imm.value);
        const int64_t rhsVal = bit::sign_extend<32, int64_t>(op->rhs.imm.value);
        const auto [result, overflow] = arm::Saturate(lhsVal + rhsVal);
        AssignImmResultWithOverflow(op->dst, result, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = m_regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);

                // Setup overflow value to 0x7FFFFFFF (if lhs is positive) or 0x80000000 (if lhs is negative)
                constexpr uint32_t maxValue = std::numeric_limits<int32_t>::max();
                auto overflowValueReg32 = m_regAlloc.GetTemporary();
                if (lhsImm) {
                    m_codegen.mov(overflowValueReg32, maxValue + bit::extract<31>(op->lhs.imm.value));
                } else {
                    m_codegen.xor_(overflowValueReg32, overflowValueReg32);
                    m_codegen.bt(dstReg32, 31);
                    m_codegen.adc(overflowValueReg32, maxValue);
                }

                m_codegen.add(dstReg32, imm);
                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                m_codegen.cmovo(dstReg32, overflowValueReg32);
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.add(tmpReg32, imm);
                SetVFromFlags();
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                m_regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                m_regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = m_regAlloc.Get(op->dst.var);

                // Setup overflow value to 0x7FFFFFFF (if lhs is positive) or 0x80000000 (if lhs is negative)
                constexpr uint32_t maxValue = std::numeric_limits<int32_t>::max();
                auto overflowValueReg32 = m_regAlloc.GetTemporary();
                m_codegen.xor_(overflowValueReg32, overflowValueReg32);
                m_codegen.bt(dstReg32, 31);
                m_codegen.adc(overflowValueReg32, maxValue);

                if (dstReg32 == lhsReg32) {
                    m_codegen.add(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    m_codegen.add(dstReg32, lhsReg32);
                } else {
                    m_codegen.mov(dstReg32, lhsReg32);
                    m_codegen.add(dstReg32, rhsReg32);
                }

                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                m_codegen.cmovo(dstReg32, overflowValueReg32);
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.add(tmpReg32, rhsReg32);
                SetVFromFlags();
            }
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRSaturatingSubtractOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).AnyOf(arm::Flags::V);

    if (lhsImm && rhsImm) {
        // Both are immediates
        const int64_t lhsVal = bit::sign_extend<32, int64_t>(op->lhs.imm.value);
        const int64_t rhsVal = bit::sign_extend<32, int64_t>(op->rhs.imm.value);
        const auto [result, overflow] = arm::Saturate(lhsVal - rhsVal);
        AssignImmResultWithOverflow(op->dst, result, overflow, setFlags);
    } else if (!lhsImm && !rhsImm) {
        // Both are variables
        auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
        auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

        if (op->dst.var.IsPresent()) {
            auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
            CopyIfDifferent(dstReg32, lhsReg32);

            // Setup overflow value to 0x7FFFFFFF (if lhs is positive) or 0x80000000 (if lhs is negative)
            constexpr uint32_t maxValue = std::numeric_limits<int32_t>::max();
            auto overflowValueReg32 = m_regAlloc.GetTemporary();
            m_codegen.xor_(overflowValueReg32, overflowValueReg32);
            m_codegen.bt(dstReg32, 31);
            m_codegen.adc(overflowValueReg32, maxValue);

            m_codegen.sub(dstReg32, rhsReg32);

            if (setFlags) {
                SetVFromFlags();
            }

            // Clamp on overflow
            m_codegen.cmovo(dstReg32, overflowValueReg32);
        } else if (setFlags) {
            m_codegen.cmp(lhsReg32, rhsReg32);
            SetVFromFlags();
        }
    } else if (rhsImm) {
        // lhs is variable, rhs is immediate
        auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
        auto rhsValue = op->rhs.imm.value;

        if (op->dst.var.IsPresent()) {
            auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);

            // Setup overflow value to 0x7FFFFFFF (if lhs is positive) or 0x80000000 (if lhs is negative)
            constexpr uint32_t maxValue = std::numeric_limits<int32_t>::max();
            auto overflowValueReg32 = m_regAlloc.GetTemporary();
            m_codegen.xor_(overflowValueReg32, overflowValueReg32);
            m_codegen.bt(dstReg32, 31);
            m_codegen.adc(overflowValueReg32, maxValue);

            m_codegen.sub(dstReg32, rhsValue);

            if (setFlags) {
                SetVFromFlags();
            }

            // Clamp on overflow
            m_codegen.cmovo(dstReg32, overflowValueReg32);
        } else if (setFlags) {
            m_codegen.cmp(lhsReg32, rhsValue);
            SetVFromFlags();
        }
    } else {
        // lhs is immediate, rhs is variable
        auto lhsValue = op->lhs.imm.value;
        auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

        if (op->dst.var.IsPresent()) {
            auto dstReg32 = m_regAlloc.Get(op->dst.var);

            // Setup overflow value to 0x7FFFFFFF (if lhs is positive) or 0x80000000 (if lhs is negative)
            constexpr uint32_t maxValue = std::numeric_limits<int32_t>::max();
            auto overflowValueReg32 = m_regAlloc.GetTemporary();
            m_codegen.mov(overflowValueReg32, maxValue + bit::extract<31>(lhsValue));

            m_codegen.mov(dstReg32, lhsValue);
            m_codegen.sub(dstReg32, rhsReg32);

            if (setFlags) {
                SetVFromFlags();
            }

            // Clamp on overflow
            m_codegen.cmovo(dstReg32, overflowValueReg32);
        } else if (setFlags) {
            auto dstReg32 = m_regAlloc.GetTemporary();
            m_codegen.mov(dstReg32, lhsValue);
            m_codegen.cmp(dstReg32, rhsReg32);
            SetVFromFlags();
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRMultiplyOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->signedMul) {
            auto result = static_cast<int32_t>(op->lhs.imm.value) * static_cast<int32_t>(op->rhs.imm.value);
            AssignImmResultWithNZ(op->dst, result, op->flags);
        } else {
            auto result = op->lhs.imm.value * op->rhs.imm.value;
            AssignImmResultWithNZ(op->dst, result, op->flags);
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = m_regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                if (op->signedMul) {
                    m_codegen.imul(dstReg32, dstReg32, static_cast<int32_t>(imm));
                } else {
                    m_codegen.imul(dstReg32.cvt64(), dstReg32.cvt64(), imm);
                }
                if (setFlags) {
                    m_codegen.test(dstReg32, dstReg32); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                if (op->signedMul) {
                    m_codegen.imul(tmpReg32, tmpReg32, static_cast<int32_t>(imm));
                } else {
                    m_codegen.imul(tmpReg32.cvt64(), tmpReg32.cvt64(), imm);
                }
                m_codegen.test(tmpReg32, tmpReg32); // We need NZ, but IMUL trashes both flags
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                m_regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                m_regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = m_regAlloc.Get(op->dst.var);

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    m_codegen.mov(dstReg32, lhsReg32);
                }
                if (op->signedMul) {
                    m_codegen.imul(dstReg32, op2Reg32);
                } else {
                    m_codegen.imul(dstReg32.cvt64(), op2Reg32.cvt64());
                }
                if (setFlags) {
                    m_codegen.test(dstReg32, dstReg32); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg32 = m_regAlloc.GetTemporary();

                m_codegen.mov(tmpReg32, lhsReg32);
                if (op->signedMul) {
                    m_codegen.imul(tmpReg32, rhsReg32);
                } else {
                    m_codegen.imul(tmpReg32.cvt64(), rhsReg32.cvt64());
                }
                m_codegen.test(tmpReg32, tmpReg32); // We need NZ, but IMUL trashes both flags
            }
        }

        if (setFlags) {
            SetNZFromFlags(op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRMultiplyLongOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->signedMul) {
            auto result =
                bit::sign_extend<32, int64_t>(op->lhs.imm.value) * bit::sign_extend<32, int64_t>(op->rhs.imm.value);
            if (op->shiftDownHalf) {
                result >>= 16ll;
            }
            AssignLongImmResultWithNZ(op->dstLo, op->dstHi, result, op->flags);
        } else {
            auto result = static_cast<uint64_t>(op->lhs.imm.value) * static_cast<uint64_t>(op->rhs.imm.value);
            if (op->shiftDownHalf) {
                result >>= 16ull;
            }
            AssignLongImmResultWithNZ(op->dstLo, op->dstHi, result, op->flags);
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = m_regAlloc.Get(var);

            if (op->dstLo.var.IsPresent() || op->dstHi.var.IsPresent()) {
                // Use dstLo or a temporary register for the 64-bit multiplication
                Xbyak::Reg64 dstReg64{};
                if (op->dstLo.var.IsPresent()) {
                    dstReg64 = m_regAlloc.ReuseAndGet(op->dstLo.var, var).cvt64();
                } else {
                    dstReg64 = m_regAlloc.GetTemporary().cvt64();
                }

                // Multiply and shift down if needed
                // If dstLo is present, the result is already in place
                if (op->signedMul) {
                    m_codegen.movsxd(dstReg64, varReg32);
                    m_codegen.imul(dstReg64, dstReg64, static_cast<int32_t>(imm));
                } else if (imm < 0x80000000) {
                    if (dstReg64.cvt32() != varReg32) {
                        m_codegen.mov(dstReg64.cvt32(), varReg32);
                    }
                    m_codegen.imul(dstReg64, dstReg64, imm);
                } else {
                    m_codegen.mov(dstReg64.cvt32(), imm);
                    m_codegen.imul(dstReg64, varReg32.cvt64());
                }
                if (op->shiftDownHalf) {
                    if (op->signedMul) {
                        m_codegen.sar(dstReg64, 16);
                    } else {
                        m_codegen.shr(dstReg64, 16);
                    }
                }

                // Store high result
                if (op->dstHi.var.IsPresent()) {
                    auto dstHiReg64 = m_regAlloc.Get(op->dstHi.var).cvt64();
                    if (CPUID::HasBMI2()) {
                        m_codegen.rorx(dstHiReg64, dstReg64, 32);
                        m_codegen.mov(dstReg64.cvt32(), dstReg64.cvt32()); // TODO: ideally MOV to another register
                    } else {
                        m_codegen.mov(dstHiReg64, dstReg64);
                        m_codegen.shr(dstHiReg64, 32);
                    }
                }

                if (setFlags) {
                    m_codegen.test(dstReg64, dstReg64); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg64 = m_regAlloc.GetTemporary().cvt64();
                if (op->signedMul) {
                    m_codegen.movsxd(tmpReg64, varReg32);
                    m_codegen.imul(tmpReg64, tmpReg64, static_cast<int32_t>(imm));
                } else {
                    m_codegen.mov(tmpReg64.cvt32(), varReg32);
                    m_codegen.imul(tmpReg64, tmpReg64, imm);
                }
                if (op->shiftDownHalf) {
                    if (op->signedMul) {
                        m_codegen.sar(tmpReg64, 16);
                    } else {
                        m_codegen.shr(tmpReg64, 16);
                    }
                }
                m_codegen.test(tmpReg64, tmpReg64); // We need NZ, but IMUL trashes both flags
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = m_regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = m_regAlloc.Get(op->rhs.var.var);

            if (op->dstLo.var.IsPresent() || op->dstHi.var.IsPresent()) {
                // Use dstLo or a temporary register for the 64-bit multiplication
                Xbyak::Reg64 dstReg64{};
                if (op->dstLo.var.IsPresent()) {
                    m_regAlloc.Reuse(op->dstLo.var, op->lhs.var.var);
                    m_regAlloc.Reuse(op->dstLo.var, op->rhs.var.var);
                    dstReg64 = m_regAlloc.Get(op->dstLo.var).cvt64();
                } else {
                    dstReg64 = m_regAlloc.GetTemporary().cvt64();
                }

                auto op2Reg32 = (dstReg64.cvt32() == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg64.cvt32() != lhsReg32 && dstReg64.cvt32() != rhsReg32) {
                    if (op->signedMul) {
                        m_codegen.movsxd(dstReg64, lhsReg32);
                    } else {
                        m_codegen.mov(dstReg64.cvt32(), lhsReg32);
                    }
                } else if (op->signedMul) {
                    m_codegen.movsxd(dstReg64, dstReg64.cvt32());
                }

                if (op->signedMul) {
                    if (op2Reg32.getIdx() != dstReg64.getIdx()) {
                        m_codegen.movsxd(op2Reg32.cvt64(), op2Reg32);
                    }
                    m_codegen.imul(dstReg64, op2Reg32.cvt64());
                } else {
                    m_codegen.imul(dstReg64, op2Reg32.cvt64());
                }
                if (op->shiftDownHalf) {
                    if (op->signedMul) {
                        m_codegen.sar(dstReg64, 16);
                    } else {
                        m_codegen.shr(dstReg64, 16);
                    }
                }

                // Store high result
                if (op->dstHi.var.IsPresent()) {
                    auto dstHiReg64 = m_regAlloc.Get(op->dstHi.var).cvt64();
                    if (CPUID::HasBMI2()) {
                        m_codegen.rorx(dstHiReg64, dstReg64, 32);
                        m_codegen.mov(dstReg64.cvt32(), dstReg64.cvt32()); // TODO: ideally MOV to another register
                    } else {
                        m_codegen.mov(dstHiReg64, dstReg64);
                        m_codegen.shr(dstHiReg64, 32);
                    }
                }

                if (setFlags) {
                    m_codegen.test(dstReg64, dstReg64); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg64 = m_regAlloc.GetTemporary().cvt64();
                auto op2Reg64 = m_regAlloc.GetTemporary().cvt64();

                if (op->signedMul) {
                    m_codegen.movsxd(tmpReg64, lhsReg32);
                    m_codegen.movsxd(op2Reg64, rhsReg32);
                    m_codegen.imul(tmpReg64, op2Reg64);
                } else {
                    m_codegen.mov(tmpReg64.cvt32(), lhsReg32);
                    m_codegen.mov(op2Reg64.cvt32(), rhsReg32);
                    m_codegen.imul(tmpReg64, op2Reg64);
                }
                m_codegen.test(tmpReg64, tmpReg64); // We need NZ, but IMUL trashes both flags
            }
        }

        if (setFlags) {
            SetNZFromFlags(op->flags);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRAddLongOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();

    // Reserve a register to contain the value 32 to be used in BMI2 shifts
    Xbyak::Reg64 shiftBy32Reg64{};
    if (CPUID::HasBMI2()) {
        shiftBy32Reg64 = m_regAlloc.GetTemporary().cvt64();
        m_codegen.mov(shiftBy32Reg64, 32);
    }

    // Compose two input variables (lo and hi) into a single 64-bit register
    auto compose64 = [&](const ir::VarOrImmArg &lo, const ir::VarOrImmArg &hi) {
        if (lo.immediate && hi.immediate) {
            // Both are immediates
            auto outReg64 = m_regAlloc.GetTemporary().cvt64();
            const uint64_t value = static_cast<uint64_t>(lo.imm.value) | (static_cast<uint64_t>(hi.imm.value) << 32ull);
            m_codegen.mov(outReg64, value);

            return outReg64;
        } else if (!lo.immediate && !hi.immediate) {
            // Both are variables
            auto loReg64 = m_regAlloc.Get(lo.var.var).cvt64();
            auto hiReg64 = m_regAlloc.Get(hi.var.var).cvt64();
            auto outReg64 = m_regAlloc.GetTemporary().cvt64();

            if (CPUID::HasBMI2()) {
                m_codegen.shlx(outReg64, hiReg64, shiftBy32Reg64);
            } else {
                m_codegen.mov(outReg64, hiReg64);
                m_codegen.shl(outReg64, 32);
            }
            m_codegen.or_(outReg64, loReg64);

            return outReg64;
        } else if (lo.immediate) {
            // lo is immediate, hi is variable
            auto hiReg64 = m_regAlloc.Get(hi.var.var).cvt64();
            auto outReg64 = m_regAlloc.GetTemporary().cvt64();

            if (outReg64 != hiReg64 && CPUID::HasBMI2()) {
                m_codegen.shlx(outReg64, hiReg64, shiftBy32Reg64);
            } else {
                CopyIfDifferent(outReg64, hiReg64);
                m_codegen.shl(outReg64, 32);
            }
            m_codegen.or_(outReg64, lo.imm.value);

            return outReg64;
        } else {
            // lo is variable, hi is immediate
            auto loReg64 = m_regAlloc.Get(lo.var.var).cvt64();
            auto outReg64 = m_regAlloc.GetTemporary().cvt64();

            if (outReg64 != loReg64 && CPUID::HasBMI2()) {
                m_codegen.shlx(outReg64, loReg64, shiftBy32Reg64);
            } else {
                CopyIfDifferent(outReg64, loReg64);
                m_codegen.shl(outReg64, 32);
            }
            m_codegen.or_(outReg64, hi.imm.value);
            m_codegen.ror(outReg64, 32);

            return outReg64;
        }
    };

    // Build 64-bit values out of the 32-bit register/immediate pairs
    // dstLo will be assigned to the first variable out of this set if possible
    auto lhsReg64 = compose64(op->lhsLo, op->lhsHi);
    auto rhsReg64 = compose64(op->rhsLo, op->rhsHi);

    // Perform the 64-bit addition into dstLo if present, or into a temporary variable otherwise
    Xbyak::Reg64 dstLoReg64{};
    if (op->dstLo.var.IsPresent() && m_regAlloc.AssignTemporary(op->dstLo.var, lhsReg64.cvt32())) {
        // Assign one of the temporary variables to dstLo
        dstLoReg64 = m_regAlloc.Get(op->dstLo.var).cvt64();
    } else {
        // Create a new temporary variable if dstLo is absent or the temporary register assignment failed
        dstLoReg64 = m_regAlloc.GetTemporary().cvt64();
        m_codegen.mov(dstLoReg64, lhsReg64);
    }
    m_codegen.add(dstLoReg64, rhsReg64);

    // Update flags if requested
    if (setFlags) {
        SetNZFromFlags(op->flags);
    }

    // Put top half of the result into dstHi if it is present
    if (op->dstHi.var.IsPresent()) {
        auto dstHiReg64 = m_regAlloc.Get(op->dstHi.var).cvt64();
        if (CPUID::HasBMI2()) {
            m_codegen.shrx(dstHiReg64, dstLoReg64, shiftBy32Reg64);
        } else {
            m_codegen.mov(dstHiReg64, dstLoReg64);
            m_codegen.shr(dstHiReg64, 32);
        }
    }

    // Clear top half of the 64-bit low destination register
    if (op->dstLo.var.IsPresent()) {
        m_codegen.mov(dstLoReg64.cvt32(), dstLoReg64.cvt32()); // TODO: ideally MOV to another register
    }
}

void x64Host::Compiler::CompileOp(const ir::IRStoreFlagsOp *op) {
    if (op->flags != arm::Flags::None) {
        const auto mask = static_cast<uint32_t>(op->flags) >> ARMflgNZCVShift;
        if (op->values.immediate) {
            const auto value = op->values.imm.value >> ARMflgNZCVShift;
            const auto ones = ARMtox64Flags(value & mask);
            const auto zeros = ARMtox64Flags(~value & mask);
            if (ones != 0) {
                m_codegen.or_(abi::kHostFlagsReg, ones);
            }
            if (zeros != 0) {
                m_codegen.and_(abi::kHostFlagsReg, ~zeros);
            }
        } else {
            auto valReg32 = m_regAlloc.Get(op->values.var.var);
            auto scratchReg32 = m_regAlloc.GetTemporary();
            m_codegen.mov(scratchReg32, valReg32);
            m_codegen.shr(scratchReg32, ARMflgNZCVShift);
            m_codegen.imul(scratchReg32, scratchReg32, ARMTox64FlagsMult);
            m_codegen.and_(scratchReg32, x64FlagsMask);
            m_codegen.and_(abi::kHostFlagsReg, ~ARMtox64Flags(mask));
            m_codegen.or_(abi::kHostFlagsReg, scratchReg32);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRLoadFlagsOp *op) {
    // Get value from srcCPSR and copy to dstCPSR, or reuse register from srcCPSR if possible
    if (op->srcCPSR.immediate) {
        auto dstReg32 = m_regAlloc.Get(op->dstCPSR.var);
        m_codegen.mov(dstReg32, op->srcCPSR.imm.value);
    } else {
        auto srcReg32 = m_regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = m_regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(dstReg32, srcReg32);
    }

    // Apply flags to dstReg32
    if (BitmaskEnum(op->flags).Any()) {
        const uint32_t cpsrMask = static_cast<uint32_t>(op->flags);
        auto dstReg32 = m_regAlloc.Get(op->dstCPSR.var);

        // Extract the host flags we need from EAX into flags
        auto flagsReg32 = m_regAlloc.GetTemporary();
        if (CPUID::HasFastPDEPAndPEXT()) {
            m_codegen.mov(flagsReg32, x64FlagsMask);
            m_codegen.pext(flagsReg32, abi::kHostFlagsReg, flagsReg32);
            m_codegen.shl(flagsReg32, 28);
        } else {
            m_codegen.imul(flagsReg32, abi::kHostFlagsReg, x64ToARMFlagsMult);
            // m_codegen.and_(flagsReg32, ARMFlagsMask);
        }
        m_codegen.and_(flagsReg32, cpsrMask); // Keep only the affected bits
        m_codegen.and_(dstReg32, ~cpsrMask);  // Clear affected bits from dst value
        m_codegen.or_(dstReg32, flagsReg32);  // Store new bits into dst value
    }
}

void x64Host::Compiler::CompileOp(const ir::IRLoadStickyOverflowOp *op) {
    if (op->setQ) {
        auto srcReg32 = m_regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = m_regAlloc.Get(op->dstCPSR.var);

        // Apply overflow flag in the Q position
        m_codegen.mov(dstReg32, abi::kHostFlagsReg); // Copy overflow flag into destination register
        m_codegen.shl(dstReg32, ARMflgQPos);         // Move Q into position
        m_codegen.or_(dstReg32, srcReg32);           // OR with srcCPSR
    } else if (op->srcCPSR.immediate) {
        auto dstReg32 = m_regAlloc.Get(op->dstCPSR.var);
        m_codegen.mov(dstReg32, op->srcCPSR.imm.value);
    } else {
        auto srcReg32 = m_regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = m_regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(dstReg32, srcReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRBranchOp *op) {
    const auto pcFieldOffset = m_stateOffsets.GPROffset(arm::GPR::PC, m_mode);
    const uint32_t instrSize = (m_thumb ? sizeof(uint16_t) : sizeof(uint32_t));
    const uint32_t pcOffset = 2 * instrSize;
    const uint32_t addrMask = ~(instrSize - 1);

    if (op->address.immediate) {
        m_codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], (op->address.imm.value & addrMask) + pcOffset);
    } else {
        auto addrReg32 = m_regAlloc.Get(op->address.var.var);
        auto tmpReg32 = m_regAlloc.GetTemporary();
        m_codegen.lea(tmpReg32, dword[addrReg32 + pcOffset]);
        m_codegen.and_(tmpReg32, addrMask);
        m_codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], tmpReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRBranchExchangeOp *op) {
    Xbyak::Label lblEnd;
    Xbyak::Label lblExchange;

    Xbyak::Reg32 addrReg32{};
    if (!op->address.immediate) {
        addrReg32 = m_regAlloc.Get(op->address.var.var);
    }

    auto pcReg32 = m_regAlloc.GetTemporary();
    const auto pcFieldOffset = m_stateOffsets.GPROffset(arm::GPR::PC, m_mode);
    const auto cpsrFieldOffset = m_stateOffsets.CPSROffset();

    const bool bxt = op->bxMode == ir::IRBranchExchangeOp::ExchangeMode::CPSRThumbFlag;
    const bool bx4 = op->bxMode == ir::IRBranchExchangeOp::ExchangeMode::L4;

    if (bxt) {
        // Determine if this is a Thumb or ARM branch based on the current CPSR T bit
        auto maskReg32 = m_regAlloc.GetTemporary();

        // Get inverted T bit for adjustments
        m_codegen.test(dword[abi::kARMStateReg + cpsrFieldOffset], ARMflgT);
        m_codegen.sete(cl); // CL is 1 when ARM, 0 when Thumb

        // Mask for PC alignment: ~1 for Thumb or ~3 for ARM
        m_codegen.mov(maskReg32, ~1); // Start with ~1
        m_codegen.shl(maskReg32, cl); // Shift left by the magic bit in CL; makes this ~3 if ARM

        if (op->address.immediate) {
            auto address = op->address.imm.value;
            auto offset = address + 4;

            // Adjust PC by +8 if ARM or +4 if Thumb
            // ARM:   PC + 1*4 + 4 = PC + 8
            // Thumb: PC + 0*4 + 4 = PC + 4
            m_codegen.movzx(pcReg32, cl);
            if (offset & 0x80000000) {
                // Handle large offsets manually
                m_codegen.shl(pcReg32, 2);
                m_codegen.add(pcReg32, offset);
            } else {
                m_codegen.lea(pcReg32, dword[pcReg32 * 4 + offset]);
            }
        } else {
            // Adjust PC by +8 if ARM or +4 if Thumb
            // ARM:   PC + 1*4 + 4 = PC + 8
            // Thumb: PC + 0*4 + 4 = PC + 4
            m_codegen.movzx(pcReg32, cl);
            m_codegen.lea(pcReg32, dword[addrReg32 + pcReg32 * 4 + 4]);
        }

        // Align the resulting address
        m_codegen.and_(pcReg32, maskReg32);
    } else {
        // Honor pre-ARMv5 branching feature if requested
        if (bx4) {
            auto &cp15 = m_context.GetARMState().GetSystemControlCoprocessor();
            if (cp15.IsPresent()) {
                auto &cp15ctl = cp15.GetControlRegister();
                const auto ctlValueOfs = offsetof(arm::cp15::ControlRegister, value);

                // Use pcReg32 as scratch register for this test
                m_codegen.mov(pcReg32.cvt64(), CastUintPtr(&cp15ctl));
                m_codegen.test(dword[pcReg32.cvt64() + ctlValueOfs], (1 << 15)); // L4 bit
                m_codegen.je(lblExchange);

                // Perform branch without exchange
                const uint32_t pcOffset = 2 * (m_thumb ? sizeof(uint16_t) : sizeof(uint32_t));
                const uint32_t addrMask = (m_thumb ? ~1 : ~3);
                if (op->address.immediate) {
                    m_codegen.mov(dword[abi::kARMStateReg + pcFieldOffset],
                                  (op->address.imm.value & addrMask) + pcOffset);
                } else {
                    m_codegen.lea(pcReg32, dword[addrReg32 + pcOffset]);
                    m_codegen.and_(pcReg32, addrMask);
                    m_codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], pcReg32);
                }
                m_codegen.jmp(lblEnd);
            }
            // If CP15 is absent, assume bit L4 is clear (the default value) -- branch and exchange
        }

        // Perform exchange
        m_codegen.L(lblExchange);
        if (op->address.immediate) {
            // Determine if this is a Thumb or ARM branch based on bit 0 of the given address
            if (op->address.imm.value & 1) {
                // Thumb branch
                m_codegen.or_(dword[abi::kARMStateReg + cpsrFieldOffset], ARMflgT);
                m_codegen.mov(pcReg32, (op->address.imm.value & ~1) + 2 * sizeof(uint16_t));
            } else {
                // ARM branch
                m_codegen.and_(dword[abi::kARMStateReg + cpsrFieldOffset], ~ARMflgT);
                m_codegen.mov(pcReg32, (op->address.imm.value & ~3) + 2 * sizeof(uint32_t));
            }
        } else {
            Xbyak::Label lblBranchARM;
            Xbyak::Label lblSetPC;

            // Determine if this is a Thumb or ARM branch based on bit 0 of the given address
            m_codegen.test(addrReg32, 1);
            m_codegen.je(lblBranchARM);

            // Thumb branch
            m_codegen.or_(dword[abi::kARMStateReg + cpsrFieldOffset], ARMflgT);
            m_codegen.lea(pcReg32, dword[addrReg32 + 2 * sizeof(uint16_t) - 1]);
            // The address always has bit 0 set, so (addr & ~1) == (addr - 1)
            // Therefore, (addr & ~1) + 4 == (addr - 1) + 4 == (addr + 3)
            // m_codegen.lea(pcReg32, dword[addrReg32 + 2 * sizeof(uint16_t)]);
            // m_codegen.and_(pcReg32, ~1);
            m_codegen.jmp(lblSetPC);

            // ARM branch
            m_codegen.L(lblBranchARM);
            m_codegen.and_(dword[abi::kARMStateReg + cpsrFieldOffset], ~ARMflgT);
            m_codegen.lea(pcReg32, dword[addrReg32 + 2 * sizeof(uint32_t)]);
            m_codegen.and_(pcReg32, ~3);

            m_codegen.L(lblSetPC);
        }
    }

    // Set PC to branch target
    m_codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], pcReg32);

    m_codegen.L(lblEnd);
}

void x64Host::Compiler::CompileOp(const ir::IRLoadCopRegisterOp *op) {
    if (!op->dstValue.var.IsPresent()) {
        return;
    }

    auto func = (op->ext) ? SystemLoadCopExtRegister : SystemLoadCopRegister;
    auto dstReg32 = m_regAlloc.Get(op->dstValue.var);
    CompileInvokeHostFunction(dstReg32, func, m_armState, (uint32_t)op->cpnum, (uint32_t)op->reg.u16);
}

void x64Host::Compiler::CompileOp(const ir::IRStoreCopRegisterOp *op) {
    auto func = (op->ext) ? SystemStoreCopExtRegister : SystemStoreCopRegister;
    if (op->srcValue.immediate) {
        CompileInvokeHostFunction(func, m_armState, (uint32_t)op->cpnum, (uint32_t)op->reg.u16, op->srcValue.imm.value);
    } else {
        auto srcReg32 = m_regAlloc.Get(op->srcValue.var.var);
        CompileInvokeHostFunction(func, m_armState, (uint32_t)op->cpnum, (uint32_t)op->reg.u16, srcReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRConstantOp *op) {
    // This instruction should be optimized away, but here's an implementation anyway
    if (op->dst.var.IsPresent()) {
        auto dstReg32 = m_regAlloc.Get(op->dst.var);
        m_codegen.mov(dstReg32, op->value);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRCopyVarOp *op) {
    // This instruction should be optimized away, but here's an implementation anyway
    if (!op->var.var.IsPresent() || !op->dst.var.IsPresent()) {
        return;
    }
    auto varReg32 = m_regAlloc.Get(op->var.var);
    auto dstReg32 = m_regAlloc.ReuseAndGet(op->dst.var, op->var.var);
    CopyIfDifferent(dstReg32, varReg32);
}

void x64Host::Compiler::CompileOp(const ir::IRGetBaseVectorAddressOp *op) {
    if (op->dst.var.IsPresent()) {
        auto &cp15 = m_armState.GetSystemControlCoprocessor();
        if (cp15.IsPresent()) {
            // Load base vector address from CP15
            auto &cp15ctl = cp15.GetControlRegister();
            const auto baseVectorAddressOfs = offsetof(arm::cp15::ControlRegister, baseVectorAddress);

            auto ctlPtrReg64 = m_regAlloc.GetTemporary().cvt64();
            auto dstReg32 = m_regAlloc.Get(op->dst.var);
            m_codegen.mov(ctlPtrReg64, CastUintPtr(&cp15ctl));
            m_codegen.mov(dstReg32, dword[ctlPtrReg64 + baseVectorAddressOfs]);
        } else {
            // Default to 00000000 if CP15 is absent
            auto dstReg32 = m_regAlloc.Get(op->dst.var);
            m_codegen.xor_(dstReg32, dstReg32);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void x64Host::Compiler::SetCFromValue(bool carry) {
    if (carry) {
        m_codegen.or_(abi::kHostFlagsReg, x64flgC);
    } else {
        m_codegen.and_(abi::kHostFlagsReg, ~x64flgC);
    }
}

void x64Host::Compiler::SetCFromFlags() {
    const auto tmpReg16 = cx;
    m_codegen.setc(GetReg8(tmpReg16));                    // Put new C into CX
    m_codegen.shl(tmpReg16, x64flgCPos);                  // Move it to the correct position
    m_codegen.and_(abi::kHostFlagsReg.cvt16(), ~x64flgC); // Clear existing C flag from AX
    m_codegen.or_(abi::kHostFlagsReg.cvt16(), tmpReg16);  // Write new C flag into AX
}

void x64Host::Compiler::SetVFromValue(bool overflow) {
    if (overflow) {
        m_codegen.mov(abi::kHostVFlagReg, 1);
    } else {
        m_codegen.xor_(abi::kHostVFlagReg, abi::kHostVFlagReg);
    }
}

void x64Host::Compiler::SetVFromFlags() {
    m_codegen.seto(abi::kHostVFlagReg);
}

void x64Host::Compiler::SetNZFromValue(uint32_t value, arm::Flags flagsMask) {
    flagsMask &= arm::Flags::NZ; // sanitize input

    const bool n = (value >> 31u);
    const bool z = (value == 0);
    const uint32_t mask = ARMtox64Flags(static_cast<uint32_t>(flagsMask));
    const uint32_t ones = ((n * x64flgN) | (z * x64flgZ)) & mask;
    const uint32_t zeros = ((!n * x64flgN) | (!z * x64flgZ)) & mask;
    if (ones != 0) {
        m_codegen.or_(abi::kHostFlagsReg, ones);
    }
    if (zeros != 0) {
        m_codegen.and_(abi::kHostFlagsReg, ~zeros);
    }
}

void x64Host::Compiler::SetNZFromValue(uint64_t value, arm::Flags flagsMask) {
    flagsMask &= arm::Flags::NZ; // sanitize input

    const bool n = (value >> 63ull);
    const bool z = (value == 0);
    const uint32_t mask = ARMtox64Flags(static_cast<uint32_t>(flagsMask));
    const uint32_t ones = ((n * x64flgN) | (z * x64flgZ)) & mask;
    const uint32_t zeros = ((!n * x64flgN) | (!z * x64flgZ)) & mask;
    if (ones != 0) {
        m_codegen.or_(abi::kHostFlagsReg, ones);
    }
    if (zeros != 0) {
        m_codegen.and_(abi::kHostFlagsReg, ~zeros);
    }
}

void x64Host::Compiler::SetNZFromReg(Xbyak::Reg32 value, arm::Flags flagsMask) {
    flagsMask &= arm::Flags::NZ; // sanitize input

    m_codegen.test(value, value); // Updates NZ, clears CV; V doesn't matter, not used here
    if (flagsMask == arm::Flags::NZ) {
        m_codegen.mov(ecx, abi::kHostFlagsReg); // Copy current flags to preserve C later
        m_codegen.lahf();                       // Load NZC; C is 0
        m_codegen.and_(ecx, x64flgC);           // Keep previous C only
        m_codegen.or_(abi::kHostFlagsReg, ecx); // Put previous C into AH; NZ is now updated and C is preserved
    } else if (flagsMask == arm::Flags::N) {
        m_codegen.sets(cl);                           // Put N into CL
        m_codegen.shl(ecx, x64flgNPos);               // Shift N to the correct place
        m_codegen.and_(abi::kHostFlagsReg, ~x64flgN); // Make room for the N flag
        m_codegen.or_(abi::kHostNZCFlagsReg, ch);     // Put N flag into AH
    } else if (flagsMask == arm::Flags::Z) {
        m_codegen.setz(cl);                           // Put Z into CL
        m_codegen.shl(ecx, x64flgZPos);               // Shift Z to the correct place
        m_codegen.and_(abi::kHostFlagsReg, ~x64flgZ); // Make room for the Z flag
        m_codegen.or_(abi::kHostNZCFlagsReg, ch);     // Put Z flag into AH
    }
}

void x64Host::Compiler::SetNZFromFlags(arm::Flags flagsMask) {
    flagsMask &= arm::Flags::NZ; // sanitize input

    if (flagsMask == arm::Flags::NZ) {
        m_codegen.clc();                        // Clear C to make way for the previous C
        m_codegen.mov(ecx, abi::kHostFlagsReg); // Copy current flags to preserve C later
        m_codegen.lahf();                       // Load NZC; C is 0
        m_codegen.and_(ecx, x64flgC);           // Keep previous C only
        m_codegen.or_(abi::kHostFlagsReg, ecx); // Put previous C into AH; NZ is now updated and C is preserved
    } else if (flagsMask == arm::Flags::N) {
        m_codegen.sets(cl);                           // Put N into CL
        m_codegen.shl(ecx, x64flgNPos);               // Shift N to the correct place
        m_codegen.and_(abi::kHostFlagsReg, ~x64flgN); // Make room for the N flag
        m_codegen.or_(abi::kHostNZCFlagsReg, ch);     // Put N flag into AH
    } else if (flagsMask == arm::Flags::Z) {
        m_codegen.setz(cl);                           // Put Z into CL
        m_codegen.shl(ecx, x64flgZPos);               // Shift Z to the correct place
        m_codegen.and_(abi::kHostFlagsReg, ~x64flgZ); // Make room for the Z flag
        m_codegen.or_(abi::kHostNZCFlagsReg, ch);     // Put Z flag into AH
    }
}

void x64Host::Compiler::SetNZCVFromValue(uint32_t value, bool carry, bool overflow, arm::Flags flagsMask) {
    const bool n = (value >> 31u);
    const bool z = (value == 0);
    const uint32_t mask = ARMtox64Flags(static_cast<uint32_t>(flagsMask));
    const uint32_t ones = ((n * x64flgN) | (z * x64flgZ) | (carry * x64flgC)) & mask;
    const uint32_t zeros = ((!n * x64flgN) | (!z * x64flgZ) | (!carry * x64flgC)) & mask;
    if (ones != 0) {
        m_codegen.or_(abi::kHostFlagsReg, ones);
    }
    if (zeros != 0) {
        m_codegen.and_(abi::kHostFlagsReg, ~zeros);
    }
    if (BitmaskEnum(flagsMask).AnyOf(arm::Flags::V)) {
        m_codegen.mov(abi::kHostVFlagReg, static_cast<uint8_t>(overflow));
    }
}

void x64Host::Compiler::SetNZCVFromFlags(arm::Flags flagsMask) {
    // Handle NZC
    auto bmFlags = BitmaskEnum(flagsMask);
    if (bmFlags.AllOf(arm::Flags::NZC)) {
        // All three flags
        m_codegen.lahf();
    } else if (bmFlags.NoneOf(arm::Flags::C)) {
        // Any combination of NZ without C
        SetNZFromFlags(flagsMask);
    } else if (bmFlags.NoneOf(arm::Flags::NZ)) {
        // C only
        m_codegen.setc(cl);                           // Put C into CL
        m_codegen.shl(ecx, x64flgCPos);               // Shift C to the correct place
        m_codegen.and_(abi::kHostFlagsReg, ~x64flgC); // Make room for the C flag
        m_codegen.or_(abi::kHostNZCFlagsReg, ch);     // Put C flag into AH
    } else if (bmFlags.AnyOf(arm::Flags::N)) {
        // C and N
        m_codegen.mov(ecx, abi::kHostFlagsReg); // Copy current flags to preserve Z later
        m_codegen.lahf();                       // Load NZC
        m_codegen.and_(ecx, x64flgZ);           // Keep previous Z only
        m_codegen.or_(abi::kHostFlagsReg, ecx); // Put previous Z into AH; NC is now updated and Z is preserved
    } else if (bmFlags.AnyOf(arm::Flags::Z)) {
        // C and Z
        m_codegen.mov(ecx, abi::kHostFlagsReg); // Copy current flags to preserve N later
        m_codegen.lahf();                       // Load NZC
        m_codegen.and_(ecx, x64flgN);           // Keep previous N only
        m_codegen.or_(abi::kHostFlagsReg, ecx); // Put previous N into AH; ZC is now updated and N is preserved
    }

    // Handle V
    if (bmFlags.AnyOf(arm::Flags::V)) {
        m_codegen.seto(abi::kHostVFlagReg);
    }
}

void x64Host::Compiler::MOVImmediate(Xbyak::Reg32 reg, uint32_t value) {
    if (value == 0) {
        m_codegen.xor_(reg, reg);
    } else {
        m_codegen.mov(reg, value);
    }
}

void x64Host::Compiler::CopyIfDifferent(Xbyak::Reg32 dst, Xbyak::Reg32 src) {
    if (dst != src) {
        m_codegen.mov(dst, src);
    }
}

void x64Host::Compiler::CopyIfDifferent(Xbyak::Reg64 dst, Xbyak::Reg64 src) {
    if (dst != src) {
        m_codegen.mov(dst, src);
    }
}

void x64Host::Compiler::AssignImmResultWithNZ(const ir::VariableArg &dst, uint32_t result, arm::Flags flagsMask) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = m_regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (BitmaskEnum(flagsMask).AnyOf(arm::Flags::N | arm::Flags::C)) {
        SetNZFromValue(result, flagsMask);
    }
}

void x64Host::Compiler::AssignImmResultWithNZCV(const ir::VariableArg &dst, uint32_t result, bool carry, bool overflow,
                                                arm::Flags flagsMask) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = m_regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (BitmaskEnum(flagsMask).Any()) {
        SetNZCVFromValue(result, carry, overflow, flagsMask);
    }
}

void x64Host::Compiler::AssignImmResultWithCarry(const ir::VariableArg &dst, uint32_t result, std::optional<bool> carry,
                                                 bool setCarry) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = m_regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setCarry && carry) {
        SetCFromValue(*carry);
    }
}

void x64Host::Compiler::AssignImmResultWithOverflow(const ir::VariableArg &dst, uint32_t result, bool overflow,
                                                    bool setOverflow) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = m_regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setOverflow) {
        SetVFromValue(overflow);
    }
}

void x64Host::Compiler::AssignLongImmResultWithNZ(const ir::VariableArg &dstLo, const ir::VariableArg &dstHi,
                                                  uint64_t result, arm::Flags flagsMask) {
    if (dstLo.var.IsPresent()) {
        auto dstReg32 = m_regAlloc.Get(dstLo.var);
        MOVImmediate(dstReg32, result);
    }
    if (dstHi.var.IsPresent()) {
        auto dstReg32 = m_regAlloc.Get(dstHi.var);
        MOVImmediate(dstReg32, result >> 32ull);
    }

    if (BitmaskEnum(flagsMask).AnyOf(arm::Flags::N | arm::Flags::Z)) {
        SetNZFromValue(result, flagsMask);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
constexpr bool is_raw_integral_v = std::is_integral_v<std::remove_cvref_t<T>>;

template <typename Base, typename Derived>
constexpr bool is_raw_base_of_v = std::is_base_of_v<Base, std::remove_cvref_t<Derived>>;

template <typename ReturnType, typename... FnArgs, typename... Args>
void x64Host::Compiler::CompileInvokeHostFunctionImpl(Xbyak::Reg dstReg, ReturnType (*fn)(FnArgs...), Args &&...args) {
    static_assert(is_raw_integral_v<ReturnType> || std::is_void_v<ReturnType> || std::is_pointer_v<ReturnType> ||
                      std::is_reference_v<ReturnType>,
                  "ReturnType must be an integral type, void, pointer or reference");

    static_assert(((is_raw_integral_v<FnArgs> || is_raw_base_of_v<Xbyak::Operand, FnArgs> ||
                    std::is_pointer_v<FnArgs> || std::is_reference_v<FnArgs>)&&...),
                  "All FnArgs must be integral types, Xbyak operands, pointers or references");

    // Save the return value and cycle count registers
    m_codegen.push(abi::kIntReturnValueReg);
    m_codegen.push(abi::kCycleCountReg);

    // Save all used volatile registers
    std::array<Xbyak::Reg64, abi::kVolatileRegs.size()> savedRegs;
    size_t savedRegsCount = 0;
    for (auto reg : abi::kVolatileRegs) {
        // The return value and cycle count registers are handled separately
        if (reg == abi::kIntReturnValueReg || reg == abi::kCycleCountReg) {
            continue;
        }

        // No need to push the destination register as it will be overwritten after the call
        if (!dstReg.isNone() && reg.getIdx() == dstReg.getIdx()) {
            continue;
        }

        // Only push allocated registers
        if (m_regAlloc.IsRegisterAllocated(reg)) {
            m_codegen.push(reg);
            savedRegs[savedRegsCount++] = reg;
        }
    }

    const uint64_t volatileRegsSize = (savedRegsCount + 2) * sizeof(uint64_t);
    const uint64_t stackAlignmentOffset = abi::Align<abi::kStackAlignmentShift>(volatileRegsSize) - volatileRegsSize;
    const uint64_t stackOffset = stackAlignmentOffset + abi::kMinStackReserveSize;

    // -----------------------------------------------------------------------------------------------------------------
    // Function call ABI registers handling
    //
    // These registers need special treatment to ensure they're not overwritten while passing arguments.
    // This algorithm constructs an assignment graph (dst <- src), then assigns registers starting from the leaves.
    // If forks are found (i.e. the same register is passed in multiple times), all of their assignments are handled
    // before moving up the graph.
    // If cycles are found, the algorithm emits a chain of xchgs to swap them.

    enum class NodeType { Unused, Leaf, NonLeaf, Cycle };
    std::array<NodeType, 16> nodes{};
    std::bitset<16> handledArgs{};           // which registers have been assigned to?
    std::array<int, 16> argRegAssignments{}; // what register is assigned to this register?
    std::array<int, 16> assignmentCount{};   // number of times this register is assigned (to handle forks)
    argRegAssignments.fill(-1);              // initialize with unknown

    // which registers are ABI function call arguments?
    constexpr auto isABIArg = [] {
        uint16_t isABIArg = 0;
        for (auto &abiReg : abi::kIntArgRegs) {
            isABIArg |= (1 << abiReg.getIdx());
        }
        return isABIArg;
    }();

    // Evaluate argument assignments and build the graph
    auto evalArgRegAssignment = [&, argIndex = 0](auto &&arg) mutable {
        using TArg = decltype(arg);
        if constexpr (is_raw_base_of_v<Xbyak::Operand, TArg>) {
            if (isABIArg & (1 << arg.getIdx())) {
                argRegAssignments[abi::kIntArgRegs[argIndex].getIdx()] = arg.getIdx();
                if (nodes[abi::kIntArgRegs[argIndex].getIdx()] != NodeType::NonLeaf) {
                    nodes[abi::kIntArgRegs[argIndex].getIdx()] = NodeType::Leaf;
                }
                nodes[arg.getIdx()] = NodeType::NonLeaf;
            }
        }
        ++argIndex;
    };
    (evalArgRegAssignment(std::forward<Args>(args)), ...);

    // Find cycles and record the necessary exchanges (in reverse order)
    std::array<std::pair<int, int>, 16> exchanges{};
    size_t exchangeCount = 0;
    for (int argReg = 0; argReg < 16; argReg++) {
        std::bitset<16> seenRegs;
        seenRegs.set(argReg);
        int assignedReg = argRegAssignments[argReg];
        while (assignedReg != -1 && nodes[assignedReg] != NodeType::Cycle) {
            if (seenRegs.test(assignedReg)) {
                int cycleStart = assignedReg;
                int reg = cycleStart;
                nodes[cycleStart] = NodeType::Cycle;
                do {
                    if (nodes[argRegAssignments[reg]] != NodeType::Cycle) {
                        exchanges[exchangeCount++] = std::make_pair(argRegAssignments[reg], reg);
                    }
                    reg = argRegAssignments[reg];
                    nodes[reg] = NodeType::Cycle;
                } while (reg != cycleStart);
                break;
            }
            seenRegs.set(assignedReg);
            assignedReg = argRegAssignments[assignedReg];
        }
    }

    // Process assignment chains, starting from leaf nodes
    for (int reg = 0; reg < 16; reg++) {
        if (nodes[reg] == NodeType::Leaf) {
            int nextReg = argRegAssignments[reg];
            while (nextReg != -1 && nodes[nextReg] != NodeType::Cycle) {
                ++assignmentCount[nextReg];
                nextReg = argRegAssignments[nextReg];
            }
        }
    }

    // Emit instruction sequences; direct assignments first, then cycle exchanges
    for (int reg = 0; reg < 16; reg++) {
        if (nodes[reg] == NodeType::Leaf) {
            int currReg = reg;
            int nextReg = argRegAssignments[reg];
            while (nextReg != -1 && nodes[nextReg] != NodeType::Cycle) {
                if (assignmentCount[nextReg] > 0) {
                    --assignmentCount[nextReg];
                }
                if (assignmentCount[currReg] == 0 || assignmentCount[nextReg] == 0) {
                    m_codegen.mov(Xbyak::Reg64{currReg}, Xbyak::Reg64{nextReg});
                    handledArgs.set(currReg);
                }
                currReg = nextReg;
                nextReg = argRegAssignments[nextReg];
            }
            if (nextReg != -1 && assignmentCount[currReg] == 0) {
                m_codegen.mov(Xbyak::Reg64{currReg}, Xbyak::Reg64{nextReg});
                handledArgs.set(currReg);
            }
        }
    }
    for (int i = exchangeCount - 1; i >= 0; i--) {
        auto &xchg = exchanges[i];
        Xbyak::Reg64 lhsReg64{xchg.first};
        Xbyak::Reg64 rhsReg64{xchg.second};
        m_codegen.xchg(lhsReg64, rhsReg64);
        handledArgs.set(xchg.first);
        handledArgs.set(xchg.second);
    }

    // -----------------------------------------------------------------------------------------------------------------

    // Process the rest of the arguments
    auto setArg = [&, argIndex = 0](auto &&arg) mutable {
        using TArg = decltype(arg);

        if (argIndex < abi::kIntArgRegs.size()) {
            auto argReg64 = abi::kIntArgRegs[argIndex];
            if constexpr (is_raw_base_of_v<Xbyak::Operand, TArg>) {
                if (!handledArgs.test(argReg64.getIdx()) && argReg64 != arg.cvt64()) {
                    m_codegen.mov(argReg64, arg.cvt64());
                }
            } else if constexpr (is_raw_integral_v<TArg>) {
                m_codegen.mov(argReg64, arg);
            } else if constexpr (std::is_pointer_v<TArg>) {
                m_codegen.mov(argReg64, CastUintPtr(arg));
            } else if constexpr (std::is_reference_v<TArg>) {
                m_codegen.mov(argReg64, CastUintPtr(&arg));
            } else if constexpr (std::is_null_pointer_v<TArg>) {
                m_codegen.mov(argReg64, CastUintPtr(arg));
            } else {
                m_codegen.mov(argReg64, arg);
            }
        } else {
            // TODO: push onto stack
            throw std::runtime_error("host function call argument-passing through the stack is unimplemented");
        }
        ++argIndex;
    };
    (setArg(std::forward<Args>(args)), ...);

    // Align stack to ABI requirement
    if (stackOffset != 0) {
        m_codegen.sub(rsp, stackOffset);
    }

    // Call host function using the return value register as a pointer
    m_codegen.mov(abi::kIntReturnValueReg, CastUintPtr(fn));
    m_codegen.call(abi::kIntReturnValueReg);

    // Undo stack alignment
    if (stackOffset != 0) {
        m_codegen.add(rsp, stackOffset);
    }

    // Pop all saved registers
    for (int i = savedRegsCount - 1; i >= 0; --i) {
        m_codegen.pop(savedRegs[i]);
    }

    // Copy result to destination register if present
    if constexpr (!std::is_void_v<ReturnType>) {
        if (!dstReg.isNone()) {
            if (dstReg.getBit() == 8) {
                m_codegen.mov(dstReg, GetReg8(abi::kIntReturnValueReg));
            } else {
                m_codegen.mov(dstReg, abi::kIntReturnValueReg.changeBit(dstReg.getBit()));
            }
        }
    }

    // Pop the cycle count and return value registers
    m_codegen.pop(abi::kCycleCountReg);
    m_codegen.pop(abi::kIntReturnValueReg);
}

} // namespace armajitto::x86_64
