#include "x86_64_compiler.hpp"

#include "armajitto/guest/arm/arithmetic.hpp"
#include "armajitto/host/x86_64/cpuid.hpp"
#include "armajitto/util/bit_ops.hpp"
#include "armajitto/util/unreachable.hpp"

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
    uint32_t value = system.MemReadHalf(address & ~1);
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
static void SystemMemWriteByte(ISystem &system, uint32_t address, uint8_t value) {
    system.MemWriteByte(address, value);
}

// ARMv4T and ARMv5TE STRH
static void SystemMemWriteHalf(ISystem &system, uint32_t address, uint16_t value) {
    system.MemWriteHalf(address & ~1, value);
}

// ARMv4T and ARMv5TE STR
static void SystemMemWriteWord(ISystem &system, uint32_t address, uint32_t value) {
    system.MemWriteWord(address & ~3, value);
}

// MRC
static uint32_t SystemLoadCopRegister(arm::State &state, uint8_t cpnum, uint16_t reg) {
    return state.GetCoprocessor(cpnum).LoadRegister(reg);
}

// MCR
static void SystemStoreCopRegister(arm::State &state, uint8_t cpnum, uint16_t reg, uint32_t value) {
    return state.GetCoprocessor(cpnum).StoreRegister(reg, value);
}

// MRC2
static uint32_t SystemLoadCopExtRegister(arm::State &state, uint8_t cpnum, uint16_t reg) {
    return state.GetCoprocessor(cpnum).LoadExtRegister(reg);
}

// MCR2
static void SystemStoreCopExtRegister(arm::State &state, uint8_t cpnum, uint16_t reg, uint32_t value) {
    return state.GetCoprocessor(cpnum).StoreExtRegister(reg, value);
}

// ---------------------------------------------------------------------------------------------------------------------

x64Host::Compiler::Compiler(Context &context, CompiledCode &compiledCode, Xbyak::CodeGenerator &codegen,
                            const ir::BasicBlock &block, std::pmr::memory_resource &alloc)
    : context(context)
    , compiledCode(compiledCode)
    , armState(context.GetARMState())
    , codegen(codegen)
    , regAlloc(codegen, alloc) {

    regAlloc.Analyze(block);
    mode = block.Location().Mode();
    thumb = block.Location().IsThumbMode();
}

void x64Host::Compiler::PreProcessOp(const ir::IROp *op) {
    regAlloc.SetInstruction(op);
}

void x64Host::Compiler::PostProcessOp(const ir::IROp *op) {
    regAlloc.ReleaseVars();
    regAlloc.ReleaseTemporaries();
}

void x64Host::Compiler::CompileIRQLineCheck() {
    const auto irqLineOffset = armState.IRQLineOffset();
    auto tmpReg8 = regAlloc.GetTemporary().cvt8();

    // Get inverted CPSR I bit
    codegen.test(abi::kHostFlagsReg, x64flgI);
    codegen.sete(tmpReg8);

    // Compare against IRQ line
    codegen.test(byte[abi::kARMStateReg + irqLineOffset], tmpReg8);

    // Jump to IRQ switch code if the IRQ line is raised and interrupts are not inhibited
    codegen.jnz(compiledCode.irqEntry);
}

void x64Host::Compiler::CompileCondCheck(arm::Condition cond, Xbyak::Label &lblCondFail) {
    switch (cond) {
    case arm::Condition::EQ: // Z=1
        codegen.sahf();
        codegen.jnz(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::NE: // Z=0
        codegen.sahf();
        codegen.jz(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::CS: // C=1
        codegen.sahf();
        codegen.jnc(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::CC: // C=0
        codegen.sahf();
        codegen.jc(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::MI: // N=1
        codegen.sahf();
        codegen.jns(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::PL: // N=0
        codegen.sahf();
        codegen.js(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::VS: // V=1
        codegen.cmp(al, 0x81);
        codegen.jno(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::VC: // V=0
        codegen.cmp(al, 0x81);
        codegen.jo(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::HI: // C=1 && Z=0
        codegen.sahf();
        codegen.cmc();
        codegen.jna(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::LS: // C=0 || Z=1
        codegen.sahf();
        codegen.cmc();
        codegen.ja(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::GE: // N=V
        codegen.cmp(al, 0x81);
        codegen.sahf();
        codegen.jnge(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::LT: // N!=V
        codegen.cmp(al, 0x81);
        codegen.sahf();
        codegen.jge(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::GT: // Z=0 && N=V
        codegen.cmp(al, 0x81);
        codegen.sahf();
        codegen.jng(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::LE: // Z=1 || N!=V
        codegen.cmp(al, 0x81);
        codegen.sahf();
        codegen.jg(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    case arm::Condition::AL: // always
        break;
    case arm::Condition::NV: // never
        // not needed as the block code is not compiled
        // codegen.jmp(lblCondFail, Xbyak::CodeGenerator::T_NEAR);
        break;
    }
}

void x64Host::Compiler::CompileTerminal(const ir::BasicBlock &block) {
    using Terminal = ir::BasicBlock::Terminal;
    const auto blockLocKey = block.Location().ToUint64();

    switch (block.GetTerminal()) {
    case Terminal::DirectLink: {
        CompileDirectLink(block.GetTerminalLocation(), blockLocKey);
        break;
    }
    case Terminal::IndirectLink: {
        auto &armState = context.GetARMState();

        // Get current CPSR and PC
        const auto cpsrOffset = armState.CPSROffset();
        const auto pcRegOffset = armState.GPROffset(arm::GPR::PC, mode);

        // Build cache key
        auto cacheKeyReg64 = regAlloc.GetTemporary().cvt64();
        codegen.mov(cacheKeyReg64, dword[abi::kARMStateReg + cpsrOffset]);
        codegen.shl(cacheKeyReg64.cvt64(), 32);
        codegen.or_(cacheKeyReg64, dword[abi::kARMStateReg + pcRegOffset]);
        regAlloc.ReleaseTemporaries(); // Temporary register not needed anymore

        // Lookup entry
        // TODO: redesign cache to not rely on this function call
        CompileInvokeHostFunction(cacheKeyReg64, CompiledCode::GetCodeForLocationTrampoline, compiledCode.blockCache,
                                  cacheKeyReg64);

        // Check for nullptr
        auto cacheEntryReg64 = cacheKeyReg64;
        codegen.test(cacheEntryReg64, cacheKeyReg64);

        // Entry not found, jump to epilog
        codegen.jz(compiledCode.epilog);

        // Entry found, jump to linked block
        codegen.jmp(cacheEntryReg64);
        break;
    }
    case Terminal::Return: CompileExit(); break;
    }
}

void x64Host::Compiler::CompileDirectLinkToSuccessor(const ir::BasicBlock &block) {
    CompileDirectLink(block.NextLocation(), block.Location().ToUint64());
}

void x64Host::Compiler::CompileExit() {
    codegen.jmp(compiledCode.epilog, Xbyak::CodeGenerator::T_NEAR);
}

void x64Host::Compiler::CompileDirectLink(LocationRef target, uint64_t blockLocKey) {
    auto it = compiledCode.blockCache.find(target.ToUint64());
    if (it != compiledCode.blockCache.end()) {
        auto code = it->second.code;

        // Jump to the compiled code's address directly
        codegen.jmp(code, Xbyak::CodeGenerator::T_NEAR);
    } else {
        // Store this code location to be patched later
        CompiledCode::PatchInfo patchInfo{.cachedBlockKey = blockLocKey, .codePos = codegen.getCurr()};

        // Exit due to cache miss; need to compile new block
        CompileExit();
        patchInfo.codeEnd = codegen.getCurr();
        compiledCode.pendingPatches[target.ToUint64()].push_back(patchInfo);
    }
}

void x64Host::Compiler::ApplyDirectLinkPatches(LocationRef target, HostCode blockCode) {
    auto itPatches = compiledCode.pendingPatches.find(target.ToUint64());
    if (itPatches != compiledCode.pendingPatches.end()) {
        for (auto &patchInfo : itPatches->second) {
            auto itPatchBlock = compiledCode.blockCache.find(patchInfo.cachedBlockKey);
            if (itPatchBlock != compiledCode.blockCache.end()) {
                // Remember current location
                auto prevSize = codegen.getSize();

                // Go to patch location
                codegen.setSize(patchInfo.codePos - codegen.getCode());

                // If target is close enough, emit up to three NOPs, otherwise emit a JMP to the target address
                auto distToTarget = (const uint8_t *)blockCode - patchInfo.codePos;
                if (distToTarget >= 1 && distToTarget <= 27 && blockCode == patchInfo.codeEnd) {
                    for (;;) {
                        if (distToTarget > 9) {
                            codegen.nop(9);
                            distToTarget -= 9;
                        } else {
                            codegen.nop(distToTarget);
                            break;
                        }
                    }
                } else {
                    codegen.jmp(blockCode, Xbyak::CodeGenerator::T_NEAR);
                }

                // Restore code generator position
                codegen.setSize(prevSize);
            }
        }
        auto &appliedPatches = compiledCode.appliedPatches[target.ToUint64()];
        appliedPatches.insert(appliedPatches.end(), itPatches->second.begin(), itPatches->second.end());
        compiledCode.pendingPatches.erase(itPatches);
    }
}

void x64Host::Compiler::RevertDirectLinkPatches(LocationRef target, HostCode blockCode) {
    auto itPatches = compiledCode.appliedPatches.find(target.ToUint64());
    if (itPatches != compiledCode.appliedPatches.end()) {
        for (auto &patchInfo : itPatches->second) {
            auto itPatchBlock = compiledCode.blockCache.find(patchInfo.cachedBlockKey);
            if (itPatchBlock != compiledCode.blockCache.end()) {
                // Remember current location
                auto prevSize = codegen.getSize();

                // Go to patch location
                codegen.setSize(patchInfo.codePos - codegen.getCode());

                // Overwrite with a jump to the epilog
                CompileExit();

                // Restore code generator position
                codegen.setSize(prevSize);
            }
        }
        auto &pendingPatches = compiledCode.pendingPatches[target.ToUint64()];
        pendingPatches.insert(pendingPatches.end(), itPatches->second.begin(), itPatches->second.end());
        compiledCode.appliedPatches.erase(itPatches);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void x64Host::Compiler::CompileOp(const ir::IRGetRegisterOp *op) {
    auto dstReg32 = regAlloc.Get(op->dst.var);
    auto offset = armState.GPROffset(op->src.gpr, op->src.Mode());
    codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::Compiler::CompileOp(const ir::IRSetRegisterOp *op) {
    auto offset = armState.GPROffset(op->dst.gpr, op->dst.Mode());
    if (op->src.immediate) {
        codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);
    } else {
        auto srcReg32 = regAlloc.Get(op->src.var.var);
        codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRGetCPSROp *op) {
    auto dstReg32 = regAlloc.Get(op->dst.var);
    auto offset = armState.CPSROffset();
    codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::Compiler::CompileOp(const ir::IRSetCPSROp *op) {
    auto offset = armState.CPSROffset();
    if (op->src.immediate) {
        codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);

        // Update I in EAX
        if (bit::test<ARMflgIPos>(op->src.imm.value)) {
            codegen.or_(abi::kHostFlagsReg, x64flgI);
        } else {
            codegen.and_(abi::kHostFlagsReg, ~x64flgI);
        }
    } else {
        auto srcReg32 = regAlloc.Get(op->src.var.var);
        codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);

        // Update I in EAX
        auto tmpReg32 = regAlloc.GetTemporary();
        codegen.mov(tmpReg32, srcReg32);
        codegen.and_(tmpReg32, (1 << ARMflgIPos));
        codegen.and_(abi::kHostFlagsReg, ~x64flgI);
        codegen.shl(tmpReg32, x64flgIPos - ARMflgIPos);
        codegen.or_(abi::kHostFlagsReg, tmpReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRGetSPSROp *op) {
    auto dstReg32 = regAlloc.Get(op->dst.var);
    auto offset = armState.SPSROffset(op->mode);
    codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::Compiler::CompileOp(const ir::IRSetSPSROp *op) {
    auto offset = armState.SPSROffset(op->mode);
    if (op->src.immediate) {
        codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);
    } else {
        auto srcReg32 = regAlloc.Get(op->src.var.var);
        codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRMemReadOp *op) {
    // TODO: handle caches, permissions, etc.
    // TODO: fast memory LUT, including TCM blocks; replace the TCM checks below
    // TODO: virtual memory, exception handling, rewriting accessors

    Xbyak::Label lblSkipTCM{};
    Xbyak::Label lblEnd{};

    auto compileRead = [this, op](Xbyak::Reg32 dstReg32, Xbyak::Reg64 addrReg64) {
        switch (op->size) {
        case ir::MemAccessSize::Byte:
            if (op->mode == ir::MemAccessMode::Signed) {
                codegen.movsx(dstReg32, byte[addrReg64]);
            } else { // aligned/unaligned
                codegen.movzx(dstReg32, byte[addrReg64]);
            }
            break;
        case ir::MemAccessSize::Half:
            if (op->mode == ir::MemAccessMode::Signed) {
                if (context.GetCPUArch() == CPUArch::ARMv4T) {
                    if (op->address.immediate) {
                        if (op->address.imm.value & 1) {
                            codegen.movsx(dstReg32, byte[addrReg64 + 1]);
                        } else {
                            codegen.movsx(dstReg32, word[addrReg64]);
                        }
                    } else {
                        Xbyak::Label lblByteRead{};
                        Xbyak::Label lblDone{};

                        auto baseAddrReg32 = regAlloc.Get(op->address.var.var);
                        codegen.test(baseAddrReg32, 1);
                        codegen.jnz(lblByteRead);

                        // Word read
                        codegen.movsx(dstReg32, word[addrReg64]);
                        codegen.jmp(lblDone);

                        // Byte read
                        codegen.L(lblByteRead);
                        codegen.movsx(dstReg32, byte[addrReg64 + 1]);

                        codegen.L(lblDone);
                    }
                } else {
                    codegen.movsx(dstReg32, word[addrReg64]);
                }
            } else if (op->mode == ir::MemAccessMode::Unaligned) {
                codegen.movzx(dstReg32, word[addrReg64]);
                if (context.GetCPUArch() == CPUArch::ARMv4T) {
                    if (op->address.immediate) {
                        const uint32_t shiftOffset = (op->address.imm.value & 1) * 8;
                        if (shiftOffset != 0) {
                            codegen.ror(dstReg32, shiftOffset);
                        }
                    } else {
                        auto baseAddrReg32 = regAlloc.Get(op->address.var.var);
                        auto shiftReg32 = regAlloc.GetRCX().cvt32();
                        codegen.mov(shiftReg32, baseAddrReg32);
                        codegen.and_(shiftReg32, 1);
                        codegen.shl(shiftReg32, 3);
                        codegen.ror(dstReg32, shiftReg32.cvt8());
                    }
                }
            } else { // aligned
                codegen.movzx(dstReg32, word[addrReg64]);
            }
            break;
        case ir::MemAccessSize::Word:
            codegen.mov(dstReg32, dword[addrReg64]);
            if (op->mode == ir::MemAccessMode::Unaligned) {
                if (op->address.immediate) {
                    const uint32_t shiftOffset = (op->address.imm.value & 3) * 8;
                    if (shiftOffset != 0) {
                        codegen.ror(dstReg32, shiftOffset);
                    }
                } else {
                    auto baseAddrReg32 = regAlloc.Get(op->address.var.var);
                    auto shiftReg32 = regAlloc.GetRCX().cvt32();
                    codegen.mov(shiftReg32, baseAddrReg32);
                    codegen.and_(shiftReg32, 3);
                    codegen.shl(shiftReg32, 3);
                    codegen.ror(dstReg32, shiftReg32.cvt8());
                }
            }
            break;
        }
    };

    if (op->dst.var.IsPresent()) {
        auto &cp15 = armState.GetSystemControlCoprocessor();
        if (cp15.IsPresent()) {
            auto &tcm = cp15.GetTCM();

            auto tcmReg64 = regAlloc.GetTemporary().cvt64();
            codegen.mov(tcmReg64, CastUintPtr(&tcm));

            // Get temporary register for the address
            auto addrReg64 = regAlloc.GetTemporary().cvt64();

            // ITCM check
            {
                constexpr auto itcmReadSizeOfs = offsetof(arm::cp15::TCM, itcmReadSize);

                Xbyak::Label lblSkipITCM{};

                // Get address
                if (op->address.immediate) {
                    codegen.mov(addrReg64.cvt32(), op->address.imm.value);
                } else {
                    auto addrReg32 = regAlloc.Get(op->address.var.var);
                    codegen.mov(addrReg64.cvt32(), addrReg32);
                }

                // Check if address is in range
                codegen.cmp(addrReg64.cvt32(), dword[tcmReg64 + itcmReadSizeOfs]);
                codegen.jae(lblSkipITCM);

                // Compute address mirror mask
                assert(std::popcount(tcm.itcmSize) == 1); // must be a power of two
                uint32_t addrMask = tcm.itcmSize - 1;
                if (op->size == ir::MemAccessSize::Half) {
                    addrMask &= ~1;
                } else if (op->size == ir::MemAccessSize::Word) {
                    addrMask &= ~3;
                }

                // Mirror and/or align address and use it as offset into the ITCM data
                codegen.and_(addrReg64, addrMask);
                codegen.mov(tcmReg64,
                            CastUintPtr(tcm.itcm)); // Use TCM pointer register as scratch for the data pointer
                codegen.add(addrReg64, tcmReg64);

                // Read from ITCM
                auto dstReg32 = regAlloc.Get(op->dst.var);
                compileRead(dstReg32, addrReg64);

                // Done!
                codegen.jmp(lblEnd);

                codegen.L(lblSkipITCM);
            }

            // DTCM check (data bus only)
            if (op->bus == ir::MemAccessBus::Data) {
                constexpr auto dtcmBaseOfs = offsetof(arm::cp15::TCM, dtcmBase);
                constexpr auto dtcmReadSizeOfs = offsetof(arm::cp15::TCM, dtcmReadSize);

                // Get address
                if (op->address.immediate) {
                    codegen.mov(addrReg64.cvt32(), op->address.imm.value);
                } else {
                    auto addrReg32 = regAlloc.Get(op->address.var.var);
                    codegen.mov(addrReg64.cvt32(), addrReg32);
                }

                // Adjust address to base offset
                codegen.sub(addrReg64.cvt32(), dword[tcmReg64 + dtcmBaseOfs]);

                // Check if address is in range
                codegen.cmp(addrReg64.cvt32(), dword[tcmReg64 + dtcmReadSizeOfs]);
                codegen.jae(lblSkipTCM);

                // Compute address mirror mask
                assert(std::popcount(tcm.dtcmSize) == 1); // must be a power of two
                uint32_t addrMask = tcm.dtcmSize - 1;
                if (op->size == ir::MemAccessSize::Half) {
                    addrMask &= ~1;
                } else if (op->size == ir::MemAccessSize::Word) {
                    addrMask &= ~3;
                }

                // Mirror and/or align address and use it as offset into the DTCM data
                codegen.and_(addrReg64, addrMask);
                codegen.mov(tcmReg64,
                            CastUintPtr(tcm.dtcm)); // Use TCM pointer register as scratch for the data pointer
                codegen.add(addrReg64, tcmReg64);

                // Read from DTCM
                auto dstReg32 = regAlloc.Get(op->dst.var);
                compileRead(dstReg32, addrReg64);

                // Done!
                codegen.jmp(lblEnd);
            }
        }
    }

    // Handle slow memory access
    codegen.L(lblSkipTCM);

    // Select parameters based on size
    // Valid combinations: aligned/signed byte, aligned/unaligned/signed half, aligned/unaligned word
    using ReadFn = uint32_t (*)(ISystem & system, uint32_t address);

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
            if (context.GetCPUArch() == CPUArch::ARMv4T) {
                readFn = SystemMemReadSignedHalfOrByte;
            } else {
                readFn = SystemMemReadSignedHalf;
            }
        } else if (op->mode == ir::MemAccessMode::Unaligned) {
            if (context.GetCPUArch() == CPUArch::ARMv4T) {
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

    auto &system = context.GetSystem();

    Xbyak::Reg dstReg32{};
    if (op->dst.var.IsPresent()) {
        dstReg32 = regAlloc.Get(op->dst.var);
    }
    if (op->address.immediate) {
        CompileInvokeHostFunction(dstReg32, readFn, system, op->address.imm.value);
    } else {
        auto addrReg32 = regAlloc.Get(op->address.var.var);
        CompileInvokeHostFunction(dstReg32, readFn, system, addrReg32);
    }

    codegen.L(lblEnd);
}

void x64Host::Compiler::CompileOp(const ir::IRMemWriteOp *op) {
    // TODO: handle caches, permissions, etc.
    // TODO: fast memory LUT, including TCM blocks; replace the TCM checks below
    // TODO: virtual memory, exception handling, rewriting accessors

    Xbyak::Label lblSkipTCM{};
    Xbyak::Label lblEnd{};

    auto &cp15 = armState.GetSystemControlCoprocessor();
    if (cp15.IsPresent()) {
        auto &tcm = cp15.GetTCM();

        auto tcmReg64 = regAlloc.GetTemporary().cvt64();
        codegen.mov(tcmReg64, CastUintPtr(&tcm));

        // Get temporary register for the address
        auto addrReg64 = regAlloc.GetTemporary().cvt64();

        // ITCM check
        {
            constexpr auto itcmWriteSizeOfs = offsetof(arm::cp15::TCM, itcmWriteSize);

            Xbyak::Label lblSkipITCM{};

            // Get address
            if (op->address.immediate) {
                codegen.mov(addrReg64.cvt32(), op->address.imm.value);
            } else {
                auto addrReg32 = regAlloc.Get(op->address.var.var);
                codegen.mov(addrReg64.cvt32(), addrReg32);
            }

            // Check if address is in range
            codegen.cmp(addrReg64.cvt32(), dword[tcmReg64 + itcmWriteSizeOfs]);
            codegen.jae(lblSkipITCM);

            // Compute address mirror mask
            assert(std::popcount(tcm.itcmSize) == 1); // must be a power of two
            uint32_t addrMask = tcm.itcmSize - 1;
            if (op->size == ir::MemAccessSize::Half) {
                addrMask &= ~1;
            } else if (op->size == ir::MemAccessSize::Word) {
                addrMask &= ~3;
            }

            // Mirror and/or align address and use it as offset into the ITCM data
            codegen.and_(addrReg64, addrMask);
            codegen.mov(tcmReg64, CastUintPtr(tcm.itcm)); // Use TCM pointer register as scratch for the data pointer
            codegen.add(addrReg64, tcmReg64);

            // Write to ITCM
            if (op->src.immediate) {
                const uint32_t value = op->src.imm.value;
                switch (op->size) {
                case ir::MemAccessSize::Byte: codegen.mov(byte[addrReg64], static_cast<uint8_t>(value)); break;
                case ir::MemAccessSize::Half: codegen.mov(word[addrReg64], static_cast<uint16_t>(value)); break;
                case ir::MemAccessSize::Word: codegen.mov(dword[addrReg64], value); break;
                }
            } else {
                auto srcReg32 = regAlloc.Get(op->src.var.var);
                switch (op->size) {
                case ir::MemAccessSize::Byte: codegen.mov(byte[addrReg64], srcReg32.cvt8()); break;
                case ir::MemAccessSize::Half: codegen.mov(word[addrReg64], srcReg32.cvt16()); break;
                case ir::MemAccessSize::Word: codegen.mov(dword[addrReg64], srcReg32.cvt32()); break;
                }
            }

            // Done!
            codegen.jmp(lblEnd);

            codegen.L(lblSkipITCM);
        }

        // DTCM check
        {
            constexpr auto dtcmBaseOfs = offsetof(arm::cp15::TCM, dtcmBase);
            constexpr auto dtcmWriteSizeOfs = offsetof(arm::cp15::TCM, dtcmWriteSize);

            // Get address
            if (op->address.immediate) {
                codegen.mov(addrReg64.cvt32(), op->address.imm.value);
            } else {
                auto addrReg32 = regAlloc.Get(op->address.var.var);
                codegen.mov(addrReg64.cvt32(), addrReg32);
            }

            // Adjust address to base offset
            codegen.sub(addrReg64.cvt32(), dword[tcmReg64 + dtcmBaseOfs]);

            // Check if address is in range
            codegen.cmp(addrReg64.cvt32(), dword[tcmReg64 + dtcmWriteSizeOfs]);
            codegen.jae(lblSkipTCM);

            // Compute address mirror mask
            assert(std::popcount(tcm.dtcmSize) == 1); // must be a power of two
            uint32_t addrMask = tcm.dtcmSize - 1;
            if (op->size == ir::MemAccessSize::Half) {
                addrMask &= ~1;
            } else if (op->size == ir::MemAccessSize::Word) {
                addrMask &= ~3;
            }

            // Mirror and/or align address and use it as offset into the DTCM data
            codegen.and_(addrReg64, addrMask);
            codegen.mov(tcmReg64, CastUintPtr(tcm.dtcm)); // Use TCM pointer register as scratch for the data pointer
            codegen.add(addrReg64, tcmReg64);

            // Write to DTCM
            if (op->src.immediate) {
                const uint32_t value = op->src.imm.value;
                switch (op->size) {
                case ir::MemAccessSize::Byte: codegen.mov(byte[addrReg64], static_cast<uint8_t>(value)); break;
                case ir::MemAccessSize::Half: codegen.mov(word[addrReg64], static_cast<uint16_t>(value)); break;
                case ir::MemAccessSize::Word: codegen.mov(dword[addrReg64], value); break;
                }
            } else {
                auto srcReg32 = regAlloc.Get(op->src.var.var);
                switch (op->size) {
                case ir::MemAccessSize::Byte: codegen.mov(byte[addrReg64], srcReg32.cvt8()); break;
                case ir::MemAccessSize::Half: codegen.mov(word[addrReg64], srcReg32.cvt16()); break;
                case ir::MemAccessSize::Word: codegen.mov(dword[addrReg64], srcReg32.cvt32()); break;
                }
            }

            // Done!
            codegen.jmp(lblEnd);
        }
    }

    // Handle slow memory access
    codegen.L(lblSkipTCM);

    auto &system = context.GetSystem();

    auto invokeFnImm8 = [&](auto fn, const ir::VarOrImmArg &address, uint8_t src) {
        if (address.immediate) {
            CompileInvokeHostFunction(fn, system, address.imm.value, src);
        } else {
            auto addrReg32 = regAlloc.Get(address.var.var);
            CompileInvokeHostFunction(fn, system, addrReg32, src);
        }
    };

    auto invokeFnImm16 = [&](auto fn, const ir::VarOrImmArg &address, uint16_t src) {
        if (address.immediate) {
            CompileInvokeHostFunction(fn, system, address.imm.value, src);
        } else {
            auto addrReg32 = regAlloc.Get(address.var.var);
            CompileInvokeHostFunction(fn, system, addrReg32, src);
        }
    };

    auto invokeFnImm32 = [&](auto fn, const ir::VarOrImmArg &address, uint32_t src) {
        if (address.immediate) {
            CompileInvokeHostFunction(fn, system, address.imm.value, src);
        } else {
            auto addrReg32 = regAlloc.Get(address.var.var);
            CompileInvokeHostFunction(fn, system, addrReg32, src);
        }
    };

    auto invokeFnReg32 = [&](auto fn, const ir::VarOrImmArg &address, ir::Variable src) {
        auto srcReg32 = regAlloc.Get(src);
        if (address.immediate) {
            CompileInvokeHostFunction(fn, system, address.imm.value, srcReg32);
        } else {
            auto addrReg32 = regAlloc.Get(address.var.var);
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

    codegen.L(lblEnd);
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
        auto shiftReg64 = regAlloc.GetRCX();
        auto valueReg64 = regAlloc.Get(op->value.var.var).cvt64();
        auto amountReg32 = regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        codegen.mov(shiftReg64, 63);
        codegen.cmp(amountReg32, 63);
        codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Get destination register
        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            auto dstReg64 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            codegen.shlx(dstReg64, valueReg64, shiftReg64);
        } else {
            Xbyak::Reg64 dstReg{};
            if (op->dst.var.IsPresent()) {
                dstReg = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
                CopyIfDifferent(dstReg, valueReg64);
            } else {
                dstReg = regAlloc.GetTemporary().cvt64();
            }

            // Compute the shift
            codegen.shl(dstReg, 32); // Shift value to the top half of the 64-bit register
            codegen.shl(dstReg, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags();
            }
            codegen.shr(dstReg, 32); // Shift value back down to the bottom half
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg64 = regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg32 = regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        codegen.mov(shiftReg64, 63);
        codegen.cmp(amountReg32, 63);
        codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Get destination register
        Xbyak::Reg64 dstReg64{};
        if (op->dst.var.IsPresent()) {
            dstReg64 = regAlloc.Get(op->dst.var).cvt64();
        } else {
            dstReg64 = regAlloc.GetTemporary().cvt64();
        }

        // Compute the shift
        codegen.mov(dstReg64, static_cast<uint64_t>(value) << 32ull);
        codegen.shl(dstReg64, shiftReg64.cvt8());
        if (op->setCarry) {
            SetCFromFlags();
        }
        codegen.shr(dstReg64, 32); // Shift value back down to the bottom half
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value;

        if (amount < 32) {
            // Get destination register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
            } else {
                dstReg32 = regAlloc.GetTemporary();
            }

            // Compute shift and update flags
            codegen.shl(dstReg32, amount);
            if (op->setCarry) {
                SetCFromFlags();
            }
        } else {
            if (amount == 32) {
                if (op->dst.var.IsPresent()) {
                    // Update carry flag before zeroing out the register
                    if (op->setCarry) {
                        codegen.bt(valueReg32, 0);
                        SetCFromFlags();
                    }

                    auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    codegen.xor_(dstReg32, dstReg32);
                } else if (op->setCarry) {
                    codegen.bt(valueReg32, 0);
                    SetCFromFlags();
                }
            } else {
                // Zero out destination
                if (op->dst.var.IsPresent()) {
                    auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    codegen.xor_(dstReg32, dstReg32);
                }
                if (op->setCarry) {
                    SetCFromValue(false);
                }
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
        auto shiftReg64 = regAlloc.GetRCX();
        auto valueReg64 = regAlloc.Get(op->value.var.var).cvt64();
        auto amountReg32 = regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        codegen.mov(shiftReg64, 63);
        codegen.cmp(amountReg32, 63);
        codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Compute the shift
        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            auto dstReg64 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            codegen.shrx(dstReg64, valueReg64, shiftReg64);
        } else if (op->dst.var.IsPresent()) {
            auto dstReg64 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            CopyIfDifferent(dstReg64, valueReg64);
            codegen.shr(dstReg64, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags();
            }
        } else if (op->setCarry) {
            codegen.dec(shiftReg64);
            codegen.bt(valueReg64, shiftReg64);
            SetCFromFlags();
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg64 = regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg32 = regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        codegen.mov(shiftReg64, 63);
        codegen.cmp(amountReg32, 63);
        codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = regAlloc.Get(op->dst.var).cvt64();
            codegen.mov(dstReg64, value);
            codegen.shr(dstReg64, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags();
            }
        } else if (op->setCarry) {
            auto valueReg64 = regAlloc.GetTemporary().cvt64();
            codegen.mov(valueReg64, (static_cast<uint64_t>(value) << 1ull));
            codegen.bt(valueReg64, shiftReg64);
            SetCFromFlags();
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value;

        if (amount < 32) {
            // Compute the shift
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
                codegen.shr(dstReg32, amount);
                if (op->setCarry) {
                    SetCFromFlags();
                }
            } else if (op->setCarry) {
                codegen.bt(valueReg32.cvt64(), amount - 1);
                SetCFromFlags();
            }
        } else if (amount == 32) {
            if (op->dst.var.IsPresent()) {
                // Update carry flag before zeroing out the register
                if (op->setCarry) {
                    codegen.bt(valueReg32, 31);
                    SetCFromFlags();
                }

                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                codegen.xor_(dstReg32, dstReg32);
            } else if (op->setCarry) {
                codegen.bt(valueReg32, 31);
                SetCFromFlags();
            }
        } else {
            // Zero out destination
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                codegen.xor_(dstReg32, dstReg32);
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
        auto [result, carry] = arm::LSR(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg32 = regAlloc.GetRCX().cvt32();
        auto valueReg32 = regAlloc.Get(op->value.var.var);
        auto amountReg32 = regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..32
        codegen.mov(shiftReg32, 32);
        codegen.cmp(amountReg32, 32);
        codegen.cmovbe(shiftReg32.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            codegen.movsxd(dstReg64, dstReg64.cvt32());
            codegen.sar(dstReg64, shiftReg32.cvt8());
            if (op->setCarry) {
                SetCFromFlags();
            }
        } else if (op->setCarry) {
            codegen.dec(shiftReg32);
            codegen.bt(valueReg32, shiftReg32);
            SetCFromFlags();
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg32 = regAlloc.GetRCX().cvt32();
        auto value = static_cast<int32_t>(op->value.imm.value);
        auto amountReg32 = regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..32
        codegen.mov(shiftReg32, 32);
        codegen.cmp(amountReg32, 32);
        codegen.cmovbe(shiftReg32.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = regAlloc.Get(op->dst.var).cvt64();
            codegen.mov(dstReg64, value);
            codegen.sar(dstReg64, shiftReg32.cvt8());
            if (op->setCarry) {
                SetCFromFlags();
            }
        } else if (op->setCarry) {
            auto valueReg64 = regAlloc.GetTemporary().cvt64();
            codegen.mov(valueReg64, (static_cast<uint64_t>(value) << 1ull));
            codegen.bt(valueReg64, shiftReg32);
            SetCFromFlags();
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = regAlloc.Get(op->value.var.var);
        auto amount = std::min(op->amount.imm.value, 32u);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            codegen.movsxd(dstReg64, valueReg32);
            codegen.sar(dstReg64, amount);
            if (op->setCarry) {
                SetCFromFlags();
            }
        } else if (op->setCarry) {
            codegen.bt(valueReg32.cvt64(), amount - 1);
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
        auto shiftReg8 = regAlloc.GetRCX().cvt8();
        auto valueReg32 = regAlloc.Get(op->value.var.var);
        auto amountReg32 = regAlloc.Get(op->amount.var.var);

        // Skip if rotation amount is zero
        Xbyak::Label lblNoRotation{};
        codegen.test(amountReg32, amountReg32);
        codegen.jz(lblNoRotation);

        {
            // Put shift amount into CL
            codegen.mov(shiftReg8, amountReg32.cvt8());

            // Put value to shift into the result register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
            } else {
                dstReg32 = regAlloc.GetTemporary();
                codegen.mov(dstReg32, valueReg32);
            }

            // Compute the shift
            codegen.ror(dstReg32, shiftReg8);
            if (op->setCarry) {
                codegen.bt(dstReg32, 31);
                SetCFromFlags();
            }
        }

        codegen.L(lblNoRotation);
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg8 = regAlloc.GetRCX().cvt8();
        auto value = op->value.imm.value;
        auto amountReg32 = regAlloc.Get(op->amount.var.var);

        // Skip if rotation amount is zero
        Xbyak::Label lblNoRotation{};
        codegen.test(amountReg32, amountReg32);
        codegen.jz(lblNoRotation);

        {
            // Put shift amount into CL
            codegen.mov(shiftReg8, amountReg32.cvt8());

            // Put value to shift into the result register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = regAlloc.Get(op->dst.var);
            } else if (op->setCarry) {
                dstReg32 = regAlloc.GetTemporary();
            }

            // Compute the shift
            codegen.mov(dstReg32, value);
            codegen.ror(dstReg32, shiftReg8);
            if (op->setCarry) {
                codegen.bt(dstReg32, 31);
                SetCFromFlags();
            }
        }

        codegen.L(lblNoRotation);
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value & 31;

        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            // Compute the shift directly into the result register
            auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            codegen.rorx(dstReg32, valueReg32, amount);
        } else {
            // Put value to shift into the result register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
            } else {
                dstReg32 = regAlloc.GetTemporary();
                codegen.mov(dstReg32, valueReg32);
            }

            // Compute the shift
            codegen.ror(dstReg32, amount);
            if (op->setCarry) {
                // If rotating by a positive multiple of 32, set the carry to the MSB
                if (amount == 0 && op->amount.imm.value != 0) {
                    codegen.bt(dstReg32, 31);
                }
                SetCFromFlags();
            }
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRRotateRightExtendedOp *op) {
    // ARM RRX works exactly the same as x86 RRX, including carry flag behavior.

    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 dstReg32{};

        if (op->value.immediate) {
            dstReg32 = regAlloc.Get(op->dst.var);
            codegen.mov(dstReg32, op->value.imm.value);
        } else {
            auto valueReg32 = regAlloc.Get(op->value.var.var);
            dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valueReg32);
        }

        codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Refresh carry flag
        codegen.rcr(dstReg32, 1);                   // Perform RRX

        if (op->setCarry) {
            SetCFromFlags();
        }
    } else if (op->setCarry) {
        if (op->value.immediate) {
            SetCFromValue(bit::test<0>(op->value.imm.value));
        } else {
            auto valueReg32 = regAlloc.Get(op->value.var.var);
            codegen.bt(valueReg32, 0);
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
        AssignImmResultWithNZ(op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                codegen.and_(dstReg32, imm);
            } else if (setFlags) {
                codegen.test(varReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    codegen.and_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    codegen.and_(dstReg32, lhsReg32);
                } else {
                    codegen.mov(dstReg32, lhsReg32);
                    codegen.and_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                codegen.test(lhsReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZFromFlags();
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
        AssignImmResultWithNZ(op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                codegen.or_(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.or_(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    codegen.or_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    codegen.or_(dstReg32, lhsReg32);
                } else {
                    codegen.mov(dstReg32, lhsReg32);
                    codegen.or_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();

                codegen.mov(tmpReg32, lhsReg32);
                codegen.or_(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZFromFlags();
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
        AssignImmResultWithNZ(op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                codegen.xor_(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.xor_(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    codegen.xor_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    codegen.xor_(dstReg32, lhsReg32);
                } else {
                    codegen.mov(dstReg32, lhsReg32);
                    codegen.xor_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();

                codegen.mov(tmpReg32, lhsReg32);
                codegen.xor_(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZFromFlags();
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
        AssignImmResultWithNZ(op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (!rhsImm) {
            // lhs is var or imm, rhs is variable
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->rhs.var.var);
                CopyIfDifferent(dstReg32, rhsReg32);
                codegen.not_(dstReg32);

                if (lhsImm) {
                    codegen.and_(dstReg32, op->lhs.imm.value);
                } else {
                    auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
                    codegen.and_(dstReg32, lhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();
                codegen.mov(tmpReg32, rhsReg32);
                codegen.not_(tmpReg32);

                if (lhsImm) {
                    codegen.test(tmpReg32, op->lhs.imm.value);
                } else {
                    auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
                    codegen.test(tmpReg32, lhsReg32);
                }
            }
        } else {
            // lhs is variable, rhs is immediate
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(dstReg32, lhsReg32);
                codegen.and_(dstReg32, ~op->rhs.imm.value);
            } else if (setFlags) {
                codegen.test(lhsReg32, ~op->rhs.imm.value);
            }
        }

        if (setFlags) {
            SetNZFromFlags();
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRCountLeadingZerosOp *op) {
    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 valReg32{};
        Xbyak::Reg32 dstReg32{};
        if (op->value.immediate) {
            valReg32 = regAlloc.GetTemporary();
            dstReg32 = regAlloc.Get(op->dst.var);
            codegen.mov(valReg32, op->value.imm.value);
        } else {
            valReg32 = regAlloc.Get(op->value.var.var);
            dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valReg32);
        }

        if (CPUID::HasLZCNT()) {
            codegen.lzcnt(dstReg32, valReg32);
        } else {
            // BSR unhelpfully returns the bit offset from the right, not left
            auto valIfZero32 = regAlloc.GetTemporary();
            codegen.mov(valIfZero32, 0xFFFFFFFF);
            codegen.bsr(dstReg32, valReg32);
            codegen.cmovz(dstReg32, valIfZero32);
            codegen.neg(dstReg32);
            codegen.add(dstReg32, 31);
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
        AssignImmResultWithNZCV(op->dst, result, carry, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                codegen.add(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.add(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = regAlloc.Get(op->dst.var);

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    codegen.mov(dstReg32, lhsReg32);
                }
                codegen.add(dstReg32, op2Reg32);
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();

                codegen.mov(tmpReg32, lhsReg32);
                codegen.add(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags();
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
            auto dstReg32 = regAlloc.Get(op->dst.var);
            codegen.mov(dstReg32, op->lhs.imm.value);

            codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            codegen.adc(dstReg32, op->rhs.imm.value);
            if (setFlags) {
                SetNZCVFromFlags();
            }
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                codegen.adc(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                codegen.adc(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = regAlloc.Get(op->dst.var);

                codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    codegen.mov(dstReg32, lhsReg32);
                }
                codegen.adc(dstReg32, op2Reg32);
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();

                codegen.mov(tmpReg32, lhsReg32);
                codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                codegen.adc(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags();
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
        AssignImmResultWithNZCV(op->dst, result, carry, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (!lhsImm) {
            // lhs is variable, rhs is var or imm
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(dstReg32, lhsReg32);

                if (rhsImm) {
                    codegen.sub(dstReg32, op->rhs.imm.value);
                } else {
                    auto rhsReg32 = regAlloc.Get(op->rhs.var.var);
                    codegen.sub(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                if (rhsImm) {
                    codegen.cmp(lhsReg32, op->rhs.imm.value);
                } else {
                    auto rhsReg32 = regAlloc.Get(op->rhs.var.var);
                    codegen.cmp(lhsReg32, rhsReg32);
                }
            }
        } else {
            // lhs is immediate, rhs is variable
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.Get(op->dst.var);
                codegen.mov(dstReg32, op->lhs.imm.value);
                codegen.sub(dstReg32, rhsReg32);
            } else if (setFlags) {
                auto lhsReg32 = regAlloc.GetTemporary();
                codegen.mov(lhsReg32, op->lhs.imm.value);
                codegen.cmp(lhsReg32, rhsReg32);
            }
        }

        if (setFlags) {
            codegen.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
            SetNZCVFromFlags();
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
            auto dstReg32 = regAlloc.Get(op->dst.var);
            codegen.mov(dstReg32, op->lhs.imm.value);

            codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            codegen.cmc();                              // Complement it
            codegen.sbb(dstReg32, op->rhs.imm.value);
            if (setFlags) {
                codegen.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
                SetNZCVFromFlags();
            }
        }
    } else {
        // At least one of the operands is a variable
        if (!lhsImm) {
            // lhs is variable, rhs is var or imm
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);

            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(dstReg32, lhsReg32);
            } else {
                dstReg32 = regAlloc.GetTemporary();
            }

            codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            codegen.cmc();                              // Complement it
            if (rhsImm) {
                codegen.sbb(dstReg32, op->rhs.imm.value);
            } else {
                auto rhsReg32 = regAlloc.Get(op->rhs.var.var);
                codegen.sbb(dstReg32, rhsReg32);
            }
        } else {
            // lhs is immediate, rhs is variable
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = regAlloc.Get(op->dst.var);
            } else {
                dstReg32 = regAlloc.GetTemporary();
            }
            codegen.mov(dstReg32, op->lhs.imm.value);
            codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            codegen.cmc();                              // Complement it
            codegen.sbb(dstReg32, rhsReg32);
        }

        if (setFlags) {
            codegen.cmc(); // Complement carry output
            SetNZCVFromFlags();
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRMoveOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg32, op->value.imm.value);
        } else {
            auto valReg32 = regAlloc.Get(op->value.var.var);
            auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valReg32);
        }

        if (setFlags) {
            auto dstReg32 = regAlloc.Get(op->dst.var);
            SetNZFromReg(dstReg32);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(op->value.imm.value);
        } else {
            auto valReg32 = regAlloc.Get(op->value.var.var);
            SetNZFromReg(valReg32);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRMoveNegatedOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg32, ~op->value.imm.value);
        } else {
            auto valReg32 = regAlloc.Get(op->value.var.var);
            auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);

            CopyIfDifferent(dstReg32, valReg32);
            codegen.not_(dstReg32);
        }

        if (setFlags) {
            auto dstReg32 = regAlloc.Get(op->dst.var);
            SetNZFromReg(dstReg32);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(~op->value.imm.value);
        } else {
            auto valReg32 = regAlloc.Get(op->value.var.var);
            auto tmpReg32 = regAlloc.GetTemporary();
            codegen.mov(tmpReg32, valReg32);
            codegen.not_(tmpReg32);
            SetNZFromReg(tmpReg32);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRSaturatingAddOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

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
            auto varReg32 = regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                codegen.add(dstReg32, imm);
                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = regAlloc.GetTemporary();
                codegen.mov(maxValReg32, std::numeric_limits<int32_t>::max());
                codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.add(tmpReg32, imm);
                SetVFromFlags();
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    codegen.add(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    codegen.add(dstReg32, lhsReg32);
                } else {
                    codegen.mov(dstReg32, lhsReg32);
                    codegen.add(dstReg32, rhsReg32);
                }

                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = regAlloc.GetTemporary();
                codegen.mov(maxValReg32, std::numeric_limits<int32_t>::max());
                codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();
                codegen.mov(tmpReg32, lhsReg32);
                codegen.add(tmpReg32, rhsReg32);
                SetVFromFlags();
            }
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRSaturatingSubtractOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const int64_t lhsVal = bit::sign_extend<32, int64_t>(op->lhs.imm.value);
        const int64_t rhsVal = bit::sign_extend<32, int64_t>(op->rhs.imm.value);
        const auto [result, overflow] = arm::Saturate(lhsVal - rhsVal);
        AssignImmResultWithOverflow(op->dst, result, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                codegen.sub(dstReg32, imm);
                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = regAlloc.GetTemporary();
                codegen.mov(maxValReg32, std::numeric_limits<int32_t>::min());
                codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.sub(tmpReg32, imm);
                SetVFromFlags();
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    codegen.sub(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    codegen.sub(dstReg32, lhsReg32);
                } else {
                    codegen.mov(dstReg32, lhsReg32);
                    codegen.sub(dstReg32, rhsReg32);
                }

                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = regAlloc.GetTemporary();
                codegen.mov(maxValReg32, std::numeric_limits<int32_t>::min());
                codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();
                codegen.mov(tmpReg32, lhsReg32);
                codegen.sub(tmpReg32, rhsReg32);
                SetVFromFlags();
            }
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
            AssignImmResultWithNZ(op->dst, result, setFlags);
        } else {
            auto result = op->lhs.imm.value * op->rhs.imm.value;
            AssignImmResultWithNZ(op->dst, result, setFlags);
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                if (op->signedMul) {
                    codegen.imul(dstReg32, dstReg32, static_cast<int32_t>(imm));
                } else {
                    codegen.imul(dstReg32.cvt64(), dstReg32.cvt64(), imm);
                }
                if (setFlags) {
                    codegen.test(dstReg32, dstReg32); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                if (op->signedMul) {
                    codegen.imul(tmpReg32, tmpReg32, static_cast<int32_t>(imm));
                } else {
                    codegen.imul(tmpReg32.cvt64(), tmpReg32.cvt64(), imm);
                }
                codegen.test(tmpReg32, tmpReg32); // We need NZ, but IMUL trashes both flags
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = regAlloc.Get(op->dst.var);

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    codegen.mov(dstReg32, lhsReg32);
                }
                if (op->signedMul) {
                    codegen.imul(dstReg32, op2Reg32);
                } else {
                    codegen.imul(dstReg32.cvt64(), op2Reg32.cvt64());
                }
                if (setFlags) {
                    codegen.test(dstReg32, dstReg32); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg32 = regAlloc.GetTemporary();

                codegen.mov(tmpReg32, lhsReg32);
                if (op->signedMul) {
                    codegen.imul(tmpReg32, rhsReg32);
                } else {
                    codegen.imul(tmpReg32.cvt64(), rhsReg32.cvt64());
                }
                codegen.test(tmpReg32, tmpReg32); // We need NZ, but IMUL trashes both flags
            }
        }

        if (setFlags) {
            SetNZFromFlags();
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
            AssignLongImmResultWithNZ(op->dstLo, op->dstHi, result, setFlags);
        } else {
            auto result = static_cast<uint64_t>(op->lhs.imm.value) * static_cast<uint64_t>(op->rhs.imm.value);
            if (op->shiftDownHalf) {
                result >>= 16ull;
            }
            AssignLongImmResultWithNZ(op->dstLo, op->dstHi, result, setFlags);
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = regAlloc.Get(var);

            if (op->dstLo.var.IsPresent() || op->dstHi.var.IsPresent()) {
                // Use dstLo or a temporary register for the 64-bit multiplication
                Xbyak::Reg64 dstReg64{};
                if (op->dstLo.var.IsPresent()) {
                    dstReg64 = regAlloc.ReuseAndGet(op->dstLo.var, var).cvt64();
                } else {
                    dstReg64 = regAlloc.GetTemporary().cvt64();
                }
                if (op->signedMul) {
                    codegen.movsxd(dstReg64, varReg32);
                } else if (dstReg64.cvt32() != varReg32) {
                    codegen.mov(dstReg64.cvt32(), varReg32);
                }

                // Multiply and shift down if needed
                // If dstLo is present, the result is already in place
                if (op->signedMul) {
                    codegen.imul(dstReg64, dstReg64, static_cast<int32_t>(imm));
                } else {
                    codegen.imul(dstReg64, dstReg64, imm);
                }
                if (op->shiftDownHalf) {
                    if (op->signedMul) {
                        codegen.sar(dstReg64, 16);
                    } else {
                        codegen.shr(dstReg64, 16);
                    }
                }

                // Store high result
                if (op->dstHi.var.IsPresent()) {
                    auto dstHiReg64 = regAlloc.Get(op->dstHi.var).cvt64();
                    if (CPUID::HasBMI2()) {
                        codegen.rorx(dstHiReg64, dstReg64, 32);
                    } else {
                        codegen.mov(dstHiReg64, dstReg64);
                        codegen.shr(dstHiReg64, 32);
                    }
                }

                if (setFlags) {
                    codegen.test(dstReg64, dstReg64); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg64 = regAlloc.GetTemporary().cvt64();
                if (op->signedMul) {
                    codegen.movsxd(tmpReg64, varReg32);
                    codegen.imul(tmpReg64, tmpReg64, static_cast<int32_t>(imm));
                } else {
                    codegen.mov(tmpReg64.cvt32(), varReg32);
                    codegen.imul(tmpReg64, tmpReg64, imm);
                }
                if (op->shiftDownHalf) {
                    if (op->signedMul) {
                        codegen.sar(tmpReg64, 16);
                    } else {
                        codegen.shr(tmpReg64, 16);
                    }
                }
                codegen.test(tmpReg64, tmpReg64); // We need NZ, but IMUL trashes both flags
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = regAlloc.Get(op->rhs.var.var);

            if (op->dstLo.var.IsPresent() || op->dstHi.var.IsPresent()) {
                // Use dstLo or a temporary register for the 64-bit multiplication
                Xbyak::Reg64 dstReg64{};
                if (op->dstLo.var.IsPresent()) {
                    regAlloc.Reuse(op->dstLo.var, op->lhs.var.var);
                    regAlloc.Reuse(op->dstLo.var, op->rhs.var.var);
                    dstReg64 = regAlloc.Get(op->dstLo.var).cvt64();
                } else {
                    dstReg64 = regAlloc.GetTemporary().cvt64();
                }

                auto op2Reg32 = (dstReg64.cvt32() == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg64.cvt32() != lhsReg32 && dstReg64.cvt32() != rhsReg32) {
                    if (op->signedMul) {
                        codegen.movsxd(dstReg64, lhsReg32);
                    } else {
                        codegen.mov(dstReg64.cvt32(), lhsReg32);
                    }
                } else if (op->signedMul) {
                    codegen.movsxd(dstReg64, dstReg64.cvt32());
                }

                if (op->signedMul) {
                    codegen.movsxd(op2Reg32.cvt64(), op2Reg32);
                    codegen.imul(dstReg64, op2Reg32.cvt64());
                } else {
                    codegen.imul(dstReg64, op2Reg32.cvt64());
                }
                if (op->shiftDownHalf) {
                    if (op->signedMul) {
                        codegen.sar(dstReg64, 16);
                    } else {
                        codegen.shr(dstReg64, 16);
                    }
                }

                // Store high result
                if (op->dstHi.var.IsPresent()) {
                    auto dstHiReg64 = regAlloc.Get(op->dstHi.var).cvt64();
                    if (CPUID::HasBMI2()) {
                        codegen.rorx(dstHiReg64, dstReg64, 32);
                    } else {
                        codegen.mov(dstHiReg64, dstReg64);
                        codegen.shr(dstHiReg64, 32);
                    }
                }

                if (setFlags) {
                    codegen.test(dstReg64, dstReg64); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg64 = regAlloc.GetTemporary().cvt64();
                auto op2Reg64 = regAlloc.GetTemporary().cvt64();

                if (op->signedMul) {
                    codegen.movsxd(tmpReg64, lhsReg32);
                    codegen.movsxd(op2Reg64, rhsReg32);
                    codegen.imul(tmpReg64, op2Reg64);
                } else {
                    codegen.mov(tmpReg64.cvt32(), lhsReg32);
                    codegen.mov(op2Reg64.cvt32(), rhsReg32);
                    codegen.imul(tmpReg64, op2Reg64);
                }
                codegen.test(tmpReg64, tmpReg64); // We need NZ, but IMUL trashes both flags
            }
        }

        if (setFlags) {
            SetNZFromFlags();
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRAddLongOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();

    // Contains the value 32 to be used in shifts
    Xbyak::Reg64 shiftBy32Reg64{};
    if (CPUID::HasBMI2()) {
        shiftBy32Reg64 = regAlloc.GetTemporary().cvt64();
        codegen.mov(shiftBy32Reg64, 32);
    }

    // Compose two input variables (lo and hi) into a single 64-bit register
    auto compose64 = [&](const ir::VarOrImmArg &lo, const ir::VarOrImmArg &hi) {
        auto outReg64 = regAlloc.GetTemporary().cvt64();
        if (lo.immediate && hi.immediate) {
            // Both are immediates
            const uint64_t value = static_cast<uint64_t>(lo.imm.value) | (static_cast<uint64_t>(hi.imm.value) << 32ull);
            codegen.mov(outReg64, value);
        } else if (!lo.immediate && !hi.immediate) {
            // Both are variables
            auto loReg64 = regAlloc.Get(lo.var.var).cvt64();
            auto hiReg64 = regAlloc.Get(hi.var.var).cvt64();

            if (CPUID::HasBMI2()) {
                codegen.shlx(outReg64, hiReg64, shiftBy32Reg64);
            } else {
                codegen.mov(outReg64, hiReg64);
                codegen.shl(outReg64, 32);
            }
            codegen.or_(outReg64, loReg64);
        } else if (lo.immediate) {
            // lo is immediate, hi is variable
            auto hiReg64 = regAlloc.Get(hi.var.var).cvt64();

            if (outReg64 != hiReg64 && CPUID::HasBMI2()) {
                codegen.shlx(outReg64, hiReg64, shiftBy32Reg64);
            } else {
                CopyIfDifferent(outReg64, hiReg64);
                codegen.shl(outReg64, 32);
            }
            codegen.or_(outReg64, lo.imm.value);
        } else {
            // lo is variable, hi is immediate
            auto loReg64 = regAlloc.Get(lo.var.var).cvt64();

            if (outReg64 != loReg64 && CPUID::HasBMI2()) {
                codegen.shlx(outReg64, loReg64, shiftBy32Reg64);
            } else {
                CopyIfDifferent(outReg64, loReg64);
                codegen.shl(outReg64, 32);
            }
            codegen.or_(outReg64, hi.imm.value);
            codegen.ror(outReg64, 32);
        }
        return outReg64;
    };

    // Build 64-bit values out of the 32-bit register/immediate pairs
    // dstLo will be assigned to the first variable out of this set if possible
    auto lhsReg64 = compose64(op->lhsLo, op->lhsHi);
    auto rhsReg64 = compose64(op->rhsLo, op->rhsHi);

    // Perform the 64-bit addition into dstLo if present, or into a temporary variable otherwise
    Xbyak::Reg64 dstLoReg64{};
    if (op->dstLo.var.IsPresent() && regAlloc.AssignTemporary(op->dstLo.var, lhsReg64.cvt32())) {
        // Assign one of the temporary variables to dstLo
        dstLoReg64 = regAlloc.Get(op->dstLo.var).cvt64();
    } else {
        // Create a new temporary variable if dstLo is absent or the temporary register assignment failed
        dstLoReg64 = regAlloc.GetTemporary().cvt64();
        codegen.mov(dstLoReg64, lhsReg64);
    }
    codegen.add(dstLoReg64, rhsReg64);

    // Update flags if requested
    if (setFlags) {
        SetNZFromFlags();
    }

    // Put top half of the result into dstHi if it is present
    if (op->dstHi.var.IsPresent()) {
        auto dstHiReg64 = regAlloc.Get(op->dstHi.var).cvt64();
        if (CPUID::HasBMI2()) {
            codegen.shrx(dstHiReg64, dstLoReg64, shiftBy32Reg64);
        } else {
            codegen.mov(dstHiReg64, dstLoReg64);
            codegen.shr(dstHiReg64, 32);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRStoreFlagsOp *op) {
    if (op->flags != arm::Flags::None) {
        const auto mask = static_cast<uint32_t>(op->flags) >> ARMflgNZCVShift;
        if (op->values.immediate) {
            const auto value = op->values.imm.value >> ARMflgNZCVShift;
            const auto ones = ((value & mask) * ARMTox64FlagsMult) & x64FlagsMask;
            const auto zeros = ((~value & mask) * ARMTox64FlagsMult) & x64FlagsMask;
            if (ones != 0) {
                codegen.or_(abi::kHostFlagsReg, ones);
            }
            if (zeros != 0) {
                codegen.and_(abi::kHostFlagsReg, ~zeros);
            }
        } else {
            auto valReg32 = regAlloc.Get(op->values.var.var);
            auto maskReg32 = regAlloc.GetTemporary();
            codegen.shr(valReg32, ARMflgNZCVShift);
            codegen.imul(valReg32, valReg32, ARMTox64FlagsMult);
            codegen.and_(valReg32, x64FlagsMask);
            codegen.mov(maskReg32, ~((mask * ARMTox64FlagsMult) & x64FlagsMask));
            codegen.and_(abi::kHostFlagsReg, maskReg32);
            codegen.or_(abi::kHostFlagsReg, valReg32);
        }
    }
}

void x64Host::Compiler::CompileOp(const ir::IRLoadFlagsOp *op) {
    // Get value from srcCPSR and copy to dstCPSR, or reuse register from srcCPSR if possible
    if (op->srcCPSR.immediate) {
        auto dstReg32 = regAlloc.Get(op->dstCPSR.var);
        codegen.mov(dstReg32, op->srcCPSR.imm.value);
    } else {
        auto srcReg32 = regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(dstReg32, srcReg32);
    }

    // Apply flags to dstReg32
    if (BitmaskEnum(op->flags).Any()) {
        const uint32_t cpsrMask = static_cast<uint32_t>(op->flags);
        auto dstReg32 = regAlloc.Get(op->dstCPSR.var);

        // Extract the host flags we need from EAX into flags
        auto flags = regAlloc.GetTemporary();
        if (CPUID::HasFastPDEPAndPEXT()) {
            codegen.mov(flags, x64FlagsMask);
            codegen.pext(flags, abi::kHostFlagsReg, flags);
            codegen.shl(flags, 28);
        } else {
            codegen.imul(flags, abi::kHostFlagsReg, x64ToARMFlagsMult);
            // codegen.and_(flags, ARMFlagsMask);
        }
        codegen.and_(flags, cpsrMask);     // Keep only the affected bits
        codegen.and_(dstReg32, ~cpsrMask); // Clear affected bits from dst value
        codegen.or_(dstReg32, flags);      // Store new bits into dst value
    }
}

void x64Host::Compiler::CompileOp(const ir::IRLoadStickyOverflowOp *op) {
    if (op->setQ) {
        auto srcReg32 = regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = regAlloc.Get(op->dstCPSR.var);

        // Apply overflow flag in the Q position
        codegen.mov(dstReg32, abi::kHostFlagsReg.cvt8()); // Copy overflow flag into destination register
        codegen.shl(dstReg32, ARMflgQPos);                // Move Q into position
        codegen.or_(dstReg32, srcReg32);                  // OR with srcCPSR
    } else if (op->srcCPSR.immediate) {
        auto dstReg32 = regAlloc.Get(op->dstCPSR.var);
        codegen.mov(dstReg32, op->srcCPSR.imm.value);
    } else {
        auto srcReg32 = regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(dstReg32, srcReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRBranchOp *op) {
    const auto pcFieldOffset = armState.GPROffset(arm::GPR::PC, mode);
    const auto instrSize = (thumb ? sizeof(uint16_t) : sizeof(uint32_t));
    const auto pcOffset = 2 * instrSize;
    const auto addrMask = ~(instrSize - 1);

    if (op->address.immediate) {
        codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], (op->address.imm.value & addrMask) + pcOffset);
    } else {
        auto addrReg32 = regAlloc.Get(op->address.var.var);
        auto tmpReg32 = regAlloc.GetTemporary();
        codegen.lea(tmpReg32, dword[addrReg32 + pcOffset]);
        codegen.and_(tmpReg32, addrMask);
        codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], tmpReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRBranchExchangeOp *op) {
    Xbyak::Label lblEnd;
    Xbyak::Label lblExchange;

    auto pcReg32 = regAlloc.GetTemporary();
    const auto pcFieldOffset = armState.GPROffset(arm::GPR::PC, mode);
    const auto cpsrFieldOffset = armState.CPSROffset();

    // Honor pre-ARMv5 branching feature if requested
    if (op->bx4) {
        auto &cp15 = context.GetARMState().GetSystemControlCoprocessor();
        if (cp15.IsPresent()) {
            auto &cp15ctl = cp15.GetControlRegister();
            const auto ctlValueOfs = offsetof(arm::cp15::ControlRegister, value);

            // Use pcReg32 as scratch register for this test
            codegen.mov(pcReg32.cvt64(), CastUintPtr(&cp15ctl));
            codegen.test(dword[pcReg32.cvt64() + ctlValueOfs], (1 << 15)); // L4 bit
            codegen.je(lblExchange);

            // Perform branch without exchange
            const auto pcOffset = 2 * (thumb ? sizeof(uint16_t) : sizeof(uint32_t));
            const auto addrMask = (thumb ? ~1 : ~3);
            if (op->address.immediate) {
                codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], (op->address.imm.value & addrMask) + pcOffset);
            } else {
                auto addrReg32 = regAlloc.Get(op->address.var.var);
                codegen.lea(pcReg32, dword[addrReg32 + pcOffset]);
                codegen.and_(pcReg32, addrMask);
                codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], pcReg32);
            }
            codegen.jmp(lblEnd);
        }
        // If CP15 is absent, assume bit L4 is clear (the default value) -- branch and exchange
    }

    // Perform exchange
    codegen.L(lblExchange);
    if (op->address.immediate) {
        // Determine if this is a Thumb or ARM branch based on bit 0 of the given address
        if (op->address.imm.value & 1) {
            // Thumb branch
            codegen.or_(dword[abi::kARMStateReg + cpsrFieldOffset], (1 << 5)); // T bit
            codegen.mov(pcReg32, (op->address.imm.value & ~1) + 2 * sizeof(uint16_t));
        } else {
            // ARM branch
            codegen.and_(dword[abi::kARMStateReg + cpsrFieldOffset], ~(1 << 5)); // T bit
            codegen.mov(pcReg32, (op->address.imm.value & ~3) + 2 * sizeof(uint32_t));
        }
    } else {
        Xbyak::Label lblBranchARM;
        Xbyak::Label lblSetPC;

        auto addrReg32 = regAlloc.Get(op->address.var.var);

        // Determine if this is a Thumb or ARM branch based on bit 0 of the given address
        codegen.test(addrReg32, 1);
        codegen.je(lblBranchARM);

        // Thumb branch
        codegen.or_(dword[abi::kARMStateReg + cpsrFieldOffset], (1 << 5));
        codegen.lea(pcReg32, dword[addrReg32 + 2 * sizeof(uint16_t) - 1]);
        // The address always has bit 0 set, so (addr & ~1) == (addr - 1)
        // Therefore, (addr & ~1) + 4 == (addr - 1) + 4 == (addr + 3)
        // codegen.lea(pcReg32, dword[addrReg32 + 2 * sizeof(uint16_t)]);
        // codegen.and_(pcReg32, ~1);
        codegen.jmp(lblSetPC);

        // ARM branch
        codegen.L(lblBranchARM);
        codegen.and_(dword[abi::kARMStateReg + cpsrFieldOffset], ~(1 << 5));
        codegen.lea(pcReg32, dword[addrReg32 + 2 * sizeof(uint32_t)]);
        codegen.and_(pcReg32, ~3);

        codegen.L(lblSetPC);
    }

    // Set PC to branch target
    codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], pcReg32);

    codegen.L(lblEnd);
}

void x64Host::Compiler::CompileOp(const ir::IRLoadCopRegisterOp *op) {
    if (!op->dstValue.var.IsPresent()) {
        return;
    }

    auto func = (op->ext) ? SystemLoadCopExtRegister : SystemLoadCopRegister;
    auto dstReg32 = regAlloc.Get(op->dstValue.var);
    CompileInvokeHostFunction(dstReg32, func, armState, op->cpnum, op->reg.u16);
}

void x64Host::Compiler::CompileOp(const ir::IRStoreCopRegisterOp *op) {
    auto func = (op->ext) ? SystemStoreCopExtRegister : SystemStoreCopRegister;
    if (op->srcValue.immediate) {
        CompileInvokeHostFunction(func, armState, op->cpnum, op->reg.u16, op->srcValue.imm.value);
    } else {
        auto srcReg32 = regAlloc.Get(op->srcValue.var.var);
        CompileInvokeHostFunction(func, armState, op->cpnum, op->reg.u16, srcReg32);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRConstantOp *op) {
    // This instruction should be optimized away, but here's an implementation anyway
    if (op->dst.var.IsPresent()) {
        auto dstReg32 = regAlloc.Get(op->dst.var);
        codegen.mov(dstReg32, op->value);
    }
}

void x64Host::Compiler::CompileOp(const ir::IRCopyVarOp *op) {
    // This instruction should be optimized away, but here's an implementation anyway
    if (!op->var.var.IsPresent() || !op->dst.var.IsPresent()) {
        return;
    }
    auto varReg32 = regAlloc.Get(op->var.var);
    auto dstReg32 = regAlloc.ReuseAndGet(op->dst.var, op->var.var);
    CopyIfDifferent(dstReg32, varReg32);
}

void x64Host::Compiler::CompileOp(const ir::IRGetBaseVectorAddressOp *op) {
    if (op->dst.var.IsPresent()) {
        auto &cp15 = armState.GetSystemControlCoprocessor();
        if (cp15.IsPresent()) {
            // Load base vector address from CP15
            auto &cp15ctl = cp15.GetControlRegister();
            const auto baseVectorAddressOfs = offsetof(arm::cp15::ControlRegister, baseVectorAddress);

            auto ctlPtrReg64 = regAlloc.GetTemporary().cvt64();
            auto dstReg32 = regAlloc.Get(op->dst.var);
            codegen.mov(ctlPtrReg64, CastUintPtr(&cp15ctl));
            codegen.mov(dstReg32, dword[ctlPtrReg64 + baseVectorAddressOfs]);
        } else {
            // Default to 00000000 if CP15 is absent
            auto dstReg32 = regAlloc.Get(op->dst.var);
            codegen.xor_(dstReg32, dstReg32);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void x64Host::Compiler::SetCFromValue(bool carry) {
    if (carry) {
        codegen.or_(abi::kHostFlagsReg, x64flgC);
    } else {
        codegen.and_(abi::kHostFlagsReg, ~x64flgC);
    }
}

void x64Host::Compiler::SetCFromFlags() {
    auto tmp16 = regAlloc.GetTemporary().cvt16();
    codegen.setc(tmp16.cvt8());                         // Put new C into a temporary register
    codegen.shl(tmp16, x64flgCPos);                     // Move it to the correct position
    codegen.and_(abi::kHostFlagsReg.cvt16(), ~x64flgC); // Clear existing C flag from AX
    codegen.or_(abi::kHostFlagsReg.cvt16(), tmp16);     // Write new C flag into AX
}

void x64Host::Compiler::SetVFromValue(bool overflow) {
    if (overflow) {
        codegen.mov(abi::kHostFlagsReg.cvt8(), 1);
    } else {
        codegen.xor_(abi::kHostFlagsReg.cvt8(), abi::kHostFlagsReg.cvt8());
    }
}

void x64Host::Compiler::SetVFromFlags() {
    codegen.seto(abi::kHostFlagsReg.cvt8());
}

void x64Host::Compiler::SetNZFromValue(uint32_t value) {
    const bool n = (value >> 31u);
    const bool z = (value == 0);
    const uint32_t ones = (n * x64flgN) | (z * x64flgZ);
    const uint32_t zeros = (!n * x64flgN) | (!z * x64flgZ);
    if (ones != 0) {
        codegen.or_(abi::kHostFlagsReg, ones);
    }
    if (zeros != 0) {
        codegen.and_(abi::kHostFlagsReg, ~zeros);
    }
}

void x64Host::Compiler::SetNZFromValue(uint64_t value) {
    const bool n = (value >> 63ull);
    const bool z = (value == 0);
    const uint32_t ones = (n * x64flgN) | (z * x64flgZ);
    const uint32_t zeros = (!n * x64flgN) | (!z * x64flgZ);
    if (ones != 0) {
        codegen.or_(abi::kHostFlagsReg, ones);
    }
    if (zeros != 0) {
        codegen.and_(abi::kHostFlagsReg, ~zeros);
    }
}

void x64Host::Compiler::SetNZFromReg(Xbyak::Reg32 value) {
    auto tmp32 = regAlloc.GetTemporary();
    codegen.test(value, value);             // Updates NZ, clears CV; V won't be changed here
    codegen.mov(tmp32, abi::kHostFlagsReg); // Copy current flags to preserve C later
    codegen.lahf();                         // Load NZC; C is 0
    codegen.and_(tmp32, x64flgC);           // Keep previous C only
    codegen.or_(abi::kHostFlagsReg, tmp32); // Put previous C into AH; NZ is now updated and C is preserved
}

void x64Host::Compiler::SetNZFromFlags() {
    auto tmp32 = regAlloc.GetTemporary();
    codegen.clc();                          // Clear C to make way for the previous C
    codegen.mov(tmp32, abi::kHostFlagsReg); // Copy current flags to preserve C later
    codegen.lahf();                         // Load NZC; C is 0
    codegen.and_(tmp32, x64flgC);           // Keep previous C only
    codegen.or_(abi::kHostFlagsReg, tmp32); // Put previous C into AH; NZ is now updated and C is preserved
}

void x64Host::Compiler::SetNZCVFromValue(uint32_t value, bool carry, bool overflow) {
    const bool n = (value >> 31u);
    const bool z = (value == 0);
    const uint32_t ones = (n * x64flgN) | (z * x64flgZ) | (carry * x64flgC);
    const uint32_t zeros = (!n * x64flgN) | (!z * x64flgZ) | (!carry * x64flgC);
    if (ones != 0) {
        codegen.or_(abi::kHostFlagsReg, ones);
    }
    if (zeros != 0) {
        codegen.and_(abi::kHostFlagsReg, ~zeros);
    }
    codegen.mov(abi::kHostFlagsReg.cvt8(), static_cast<uint8_t>(overflow));
}

void x64Host::Compiler::SetNZCVFromFlags() {
    codegen.lahf();
    codegen.seto(abi::kHostFlagsReg.cvt8());
}

void x64Host::Compiler::MOVImmediate(Xbyak::Reg32 reg, uint32_t value) {
    if (value == 0) {
        codegen.xor_(reg, reg);
    } else {
        codegen.mov(reg, value);
    }
}

void x64Host::Compiler::CopyIfDifferent(Xbyak::Reg32 dst, Xbyak::Reg32 src) {
    if (dst != src) {
        codegen.mov(dst, src);
    }
}

void x64Host::Compiler::CopyIfDifferent(Xbyak::Reg64 dst, Xbyak::Reg64 src) {
    if (dst != src) {
        codegen.mov(dst, src);
    }
}

void x64Host::Compiler::AssignImmResultWithNZ(const ir::VariableArg &dst, uint32_t result, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setFlags) {
        SetNZFromValue(result);
    }
}

void x64Host::Compiler::AssignImmResultWithNZCV(const ir::VariableArg &dst, uint32_t result, bool carry, bool overflow,
                                                bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setFlags) {
        SetNZCVFromValue(result, carry, overflow);
    }
}

void x64Host::Compiler::AssignImmResultWithCarry(const ir::VariableArg &dst, uint32_t result, std::optional<bool> carry,
                                                 bool setCarry) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setCarry && carry) {
        SetCFromValue(*carry);
    }
}

void x64Host::Compiler::AssignImmResultWithOverflow(const ir::VariableArg &dst, uint32_t result, bool overflow,
                                                    bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setFlags) {
        SetVFromValue(overflow);
    }
}

void x64Host::Compiler::AssignLongImmResultWithNZ(const ir::VariableArg &dstLo, const ir::VariableArg &dstHi,
                                                  uint64_t result, bool setFlags) {
    if (dstLo.var.IsPresent()) {
        auto dstReg32 = regAlloc.Get(dstLo.var);
        MOVImmediate(dstReg32, result);
    }
    if (dstHi.var.IsPresent()) {
        auto dstReg32 = regAlloc.Get(dstHi.var);
        MOVImmediate(dstReg32, result >> 32ull);
    }

    if (setFlags) {
        SetNZFromValue(result);
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

    // Save the return value register
    codegen.push(abi::kIntReturnValueReg);

    // Save all used volatile registers
    std::vector<Xbyak::Reg64> savedRegs;
    for (auto reg : abi::kVolatileRegs) {
        // The return register is handled on its own
        if (reg == abi::kIntReturnValueReg) {
            continue;
        }

        // Only push allocated registers
        if (regAlloc.IsRegisterAllocated(reg)) {
            codegen.push(reg);
            savedRegs.push_back(reg);
        }
    }

    const uint64_t volatileRegsSize = (savedRegs.size() + 1) * sizeof(uint64_t);
    const uint64_t stackAlignmentOffset = abi::Align<abi::kStackAlignmentShift>(volatileRegsSize) - volatileRegsSize;

    // Put arguments in the corresponding argument registers
    size_t argIndex = 0;
    auto setArg = [&](auto &&arg) {
        using TArg = decltype(arg);

        if (argIndex < abi::kIntArgRegs.size()) {
            auto argReg64 = abi::kIntArgRegs[argIndex];
            if constexpr (is_raw_base_of_v<Xbyak::Operand, TArg>) {
                if (argReg64 != arg.cvt64()) {
                    codegen.mov(argReg64, arg.cvt64());
                }
            } else if constexpr (is_raw_integral_v<TArg>) {
                codegen.mov(argReg64, arg);
            } else if constexpr (std::is_pointer_v<TArg>) {
                codegen.mov(argReg64, CastUintPtr(arg));
            } else if constexpr (std::is_reference_v<TArg>) {
                codegen.mov(argReg64, CastUintPtr(&arg));
            } else {
                codegen.mov(argReg64, arg);
            }
        } else {
            // TODO: push onto stack
            throw std::runtime_error("host function call argument-passing through the stack is unimplemented");
        }
        ++argIndex;
    };
    (setArg(std::forward<Args>(args)), ...);

    // Align stack to ABI requirement
    if (stackAlignmentOffset != 0) {
        codegen.sub(rsp, stackAlignmentOffset);
    }

    // Call host function using the return value register as a pointer
    codegen.mov(abi::kIntReturnValueReg, CastUintPtr(fn));
    codegen.call(abi::kIntReturnValueReg);

    // Undo stack alignment
    if (stackAlignmentOffset != 0) {
        codegen.add(rsp, stackAlignmentOffset);
    }

    // Pop all saved registers
    for (auto it = savedRegs.rbegin(); it != savedRegs.rend(); it++) {
        codegen.pop(*it);
    }

    // Copy result to destination register if present
    if constexpr (!std::is_void_v<ReturnType>) {
        if (!dstReg.isNone()) {
            codegen.mov(dstReg, abi::kIntReturnValueReg.changeBit(dstReg.getBit()));
        }
    }

    // Pop the return value register
    codegen.pop(abi::kIntReturnValueReg);
}

} // namespace armajitto::x86_64
