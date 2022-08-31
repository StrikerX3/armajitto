#include "armajitto/host/x86_64/x86_64_host.hpp"

#include "armajitto/guest/arm/arithmetic.hpp"
#include "armajitto/host/x86_64/cpuid.hpp"
#include "armajitto/ir/ops/ir_ops_visitor.hpp"
#include "armajitto/util/bit_ops.hpp"
#include "armajitto/util/pointer_cast.hpp"
#include "armajitto/util/unreachable.hpp"

#include "abi.hpp"
#include "vtune.hpp"
#include "x86_64_compiler.hpp"

#include <limits>

namespace armajitto::x86_64 {

// From Dynarmic:

// This is a constant used to create the x64 flags format from the ARM format.
// NZCV * multiplier: NZCV0NZCV000NZCV
// x64_flags format:  NZ-----C-------V
constexpr uint32_t ARMTox64FlagsMult = 0x1081;

// This is a constant used to create the ARM format from the x64 flags format.
constexpr uint32_t x64ToARMFlagsMult = 0x1021'0000;

constexpr uint32_t ARMflgQPos = 27u;
constexpr uint32_t ARMflgNZCVShift = 28u;

constexpr uint32_t x64flgNPos = 15u;
constexpr uint32_t x64flgZPos = 14u;
constexpr uint32_t x64flgCPos = 8u;
constexpr uint32_t x64flgVPos = 0u;

constexpr uint32_t x64flgN = (1u << x64flgNPos);
constexpr uint32_t x64flgZ = (1u << x64flgZPos);
constexpr uint32_t x64flgC = (1u << x64flgCPos);
constexpr uint32_t x64flgV = (1u << x64flgVPos);

constexpr uint32_t x64FlagsMask = x64flgN | x64flgZ | x64flgC | x64flgV;

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

x64Host::x64Host(Context &context)
    : Host(context)
    , m_codegen(Xbyak::DEFAULT_MAX_CODE_SIZE, nullptr, &m_alloc) {

    CompileProlog();
    CompileEpilog();
}

HostCode x64Host::Compile(ir::BasicBlock &block) {
    const auto blockLocKey = block.Location().ToUint64();
    auto &cachedBlock = m_blockCache[blockLocKey];
    Compiler compiler{m_codegen};
    auto &codegen = compiler.codegen;

    compiler.Analyze(block);

    auto fnPtr = codegen.getCurr<HostCode>();
    codegen.setProtectModeRW();

    Xbyak::Label lblCondFail{};
    Xbyak::Label lblCondPass{};

    CompileCondCheck(compiler, block.Condition(), lblCondFail);

    // Compile block code
    auto *op = block.Head();
    while (op != nullptr) {
        compiler.PreProcessOp(op);
        ir::VisitIROp(op, [this, &compiler](const auto *op) -> void { CompileOp(compiler, op); });
        compiler.PostProcessOp(op);
        op = op->Next();
    }

    // Skip over condition fail block
    codegen.jmp(lblCondPass);

    // Update PC if condition fails
    codegen.L(lblCondFail);
    const auto pcRegOffset = m_armState.GPROffset(arm::GPR::PC, compiler.mode);
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
    m_codegen.mov(flagsReg, dword[CastUintPtr(&m_armState.CPSR())]);
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
    m_codegen.mov(funcAddr, abi::kIntArgRegs[0]);               // Get block code pointer from 1st arg
    m_codegen.mov(abi::kARMStateReg, CastUintPtr(&m_armState)); // Set ARM state pointer
    m_codegen.jmp(funcAddr);                                    // Jump to block code

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

void x64Host::CompileCondCheck(Compiler &compiler, arm::Condition cond, Xbyak::Label &lblCondFail) {
    auto &codegen = compiler.codegen;
    switch (cond) {
    case arm::Condition::EQ: // Z=1
        codegen.sahf();
        codegen.jnz(lblCondFail);
        break;
    case arm::Condition::NE: // Z=0
        codegen.sahf();
        codegen.jz(lblCondFail);
        break;
    case arm::Condition::CS: // C=1
        codegen.sahf();
        codegen.jnc(lblCondFail);
        break;
    case arm::Condition::CC: // C=0
        codegen.sahf();
        codegen.jc(lblCondFail);
        break;
    case arm::Condition::MI: // N=1
        codegen.sahf();
        codegen.jns(lblCondFail);
        break;
    case arm::Condition::PL: // N=0
        codegen.sahf();
        codegen.js(lblCondFail);
        break;
    case arm::Condition::VS: // V=1
        codegen.cmp(al, 0x81);
        codegen.jno(lblCondFail);
        break;
    case arm::Condition::VC: // V=0
        codegen.cmp(al, 0x81);
        codegen.jo(lblCondFail);
        break;
    case arm::Condition::HI: // C=1 && Z=0
        codegen.sahf();
        codegen.cmc();
        codegen.jna(lblCondFail);
        break;
    case arm::Condition::LS: // C=0 || Z=1
        codegen.sahf();
        codegen.cmc();
        codegen.ja(lblCondFail);
        break;
    case arm::Condition::GE: // N=V
        codegen.cmp(al, 0x81);
        codegen.sahf();
        codegen.jnge(lblCondFail);
        break;
    case arm::Condition::LT: // N!=V
        codegen.cmp(al, 0x81);
        codegen.sahf();
        codegen.jge(lblCondFail);
        break;
    case arm::Condition::GT: // Z=0 && N=V
        codegen.cmp(al, 0x81);
        codegen.sahf();
        codegen.jng(lblCondFail);
        break;
    case arm::Condition::LE: // Z=1 || N!=V
        codegen.cmp(al, 0x81);
        codegen.sahf();
        codegen.jg(lblCondFail);
        break;
    case arm::Condition::AL: // always
        break;
    case arm::Condition::NV: // never
        codegen.jmp(lblCondFail);
        break;
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetRegisterOp *op) {
    auto &codegen = compiler.codegen;
    auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.GPROffset(op->src.gpr, op->src.Mode());
    codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetRegisterOp *op) {
    auto &codegen = compiler.codegen;
    auto offset = m_armState.GPROffset(op->dst.gpr, op->dst.Mode());
    if (op->src.immediate) {
        codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
        codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetCPSROp *op) {
    auto &codegen = compiler.codegen;
    auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.CPSROffset();
    codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetCPSROp *op) {
    auto &codegen = compiler.codegen;
    auto offset = m_armState.CPSROffset();
    if (op->src.immediate) {
        codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
        codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetSPSROp *op) {
    auto &codegen = compiler.codegen;
    auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.SPSROffset(op->mode);
    codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetSPSROp *op) {
    auto &codegen = compiler.codegen;
    auto offset = m_armState.SPSROffset(op->mode);
    if (op->src.immediate) {
        codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
        codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMemReadOp *op) {
    // TODO: handle caches, permissions, etc.
    // TODO: fast memory LUT, including TCM blocks; replace the TCM checks below
    // TODO: virtual memory, exception handling, rewriting accessors
    auto &codegen = compiler.codegen;

    Xbyak::Label lblSkipTCM{};
    Xbyak::Label lblEnd{};

    auto compileRead = [this, op, &compiler, &codegen](Xbyak::Reg32 dstReg32, Xbyak::Reg64 addrReg64) {
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
                if (m_context.GetCPUArch() == CPUArch::ARMv4T) {
                    if (op->address.immediate) {
                        if (op->address.imm.value & 1) {
                            codegen.movsx(dstReg32, byte[addrReg64 + 1]);
                        } else {
                            codegen.movsx(dstReg32, word[addrReg64]);
                        }
                    } else {
                        Xbyak::Label lblByteRead{};
                        Xbyak::Label lblDone{};

                        auto baseAddrReg32 = compiler.regAlloc.Get(op->address.var.var);
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
                if (m_context.GetCPUArch() == CPUArch::ARMv4T) {
                    if (op->address.immediate) {
                        const uint32_t shiftOffset = (op->address.imm.value & 1) * 8;
                        if (shiftOffset != 0) {
                            codegen.ror(dstReg32, shiftOffset);
                        }
                    } else {
                        auto baseAddrReg32 = compiler.regAlloc.Get(op->address.var.var);
                        auto shiftReg32 = compiler.regAlloc.GetRCX().cvt32();
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
                    auto baseAddrReg32 = compiler.regAlloc.Get(op->address.var.var);
                    auto shiftReg32 = compiler.regAlloc.GetRCX().cvt32();
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
        auto &cp15 = m_armState.GetSystemControlCoprocessor();
        if (cp15.IsPresent()) {
            auto &tcm = cp15.GetTCM();

            auto tcmReg64 = compiler.regAlloc.GetTemporary().cvt64();
            codegen.mov(tcmReg64, CastUintPtr(&tcm));

            // Get temporary register for the address
            auto addrReg64 = compiler.regAlloc.GetTemporary().cvt64();

            // ITCM check
            {
                constexpr auto itcmReadSizeOfs = offsetof(arm::cp15::TCM, itcmReadSize);

                Xbyak::Label lblSkipITCM{};

                // Get address
                if (op->address.immediate) {
                    codegen.mov(addrReg64.cvt32(), op->address.imm.value);
                } else {
                    auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
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
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
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
                    auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
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
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
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

    Xbyak::Reg dstReg32{};
    if (op->dst.var.IsPresent()) {
        dstReg32 = compiler.regAlloc.Get(op->dst.var);
    }
    if (op->address.immediate) {
        CompileInvokeHostFunction(compiler, dstReg32, readFn, m_system, op->address.imm.value);
    } else {
        auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
        CompileInvokeHostFunction(compiler, dstReg32, readFn, m_system, addrReg32);
    }

    codegen.L(lblEnd);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMemWriteOp *op) {
    // TODO: handle caches, permissions, etc.
    // TODO: fast memory LUT, including TCM blocks; replace the TCM checks below
    // TODO: virtual memory, exception handling, rewriting accessors
    auto &codegen = compiler.codegen;

    Xbyak::Label lblSkipTCM{};
    Xbyak::Label lblEnd{};

    auto &cp15 = m_armState.GetSystemControlCoprocessor();
    if (cp15.IsPresent()) {
        auto &tcm = cp15.GetTCM();

        auto tcmReg64 = compiler.regAlloc.GetTemporary().cvt64();
        codegen.mov(tcmReg64, CastUintPtr(&tcm));

        // Get temporary register for the address
        auto addrReg64 = compiler.regAlloc.GetTemporary().cvt64();

        // ITCM check
        {
            constexpr auto itcmWriteSizeOfs = offsetof(arm::cp15::TCM, itcmWriteSize);

            Xbyak::Label lblSkipITCM{};

            // Get address
            if (op->address.immediate) {
                codegen.mov(addrReg64.cvt32(), op->address.imm.value);
            } else {
                auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
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
                switch (op->size) {
                case ir::MemAccessSize::Byte: codegen.mov(byte[addrReg64], op->src.imm.value); break;
                case ir::MemAccessSize::Half: codegen.mov(word[addrReg64], op->src.imm.value); break;
                case ir::MemAccessSize::Word: codegen.mov(dword[addrReg64], op->src.imm.value); break;
                }
            } else {
                auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
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
                auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
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
                switch (op->size) {
                case ir::MemAccessSize::Byte: codegen.mov(byte[addrReg64], op->src.imm.value); break;
                case ir::MemAccessSize::Half: codegen.mov(word[addrReg64], op->src.imm.value); break;
                case ir::MemAccessSize::Word: codegen.mov(dword[addrReg64], op->src.imm.value); break;
                }
            } else {
                auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
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

    auto invokeFnImm8 = [&](auto fn, const ir::VarOrImmArg &address, uint8_t src) {
        if (address.immediate) {
            CompileInvokeHostFunction(compiler, fn, m_system, address.imm.value, src);
        } else {
            auto addrReg32 = compiler.regAlloc.Get(address.var.var);
            CompileInvokeHostFunction(compiler, fn, m_system, addrReg32, src);
        }
    };

    auto invokeFnImm16 = [&](auto fn, const ir::VarOrImmArg &address, uint16_t src) {
        if (address.immediate) {
            CompileInvokeHostFunction(compiler, fn, m_system, address.imm.value, src);
        } else {
            auto addrReg32 = compiler.regAlloc.Get(address.var.var);
            CompileInvokeHostFunction(compiler, fn, m_system, addrReg32, src);
        }
    };

    auto invokeFnImm32 = [&](auto fn, const ir::VarOrImmArg &address, uint32_t src) {
        if (address.immediate) {
            CompileInvokeHostFunction(compiler, fn, m_system, address.imm.value, src);
        } else {
            auto addrReg32 = compiler.regAlloc.Get(address.var.var);
            CompileInvokeHostFunction(compiler, fn, m_system, addrReg32, src);
        }
    };

    auto invokeFnReg32 = [&](auto fn, const ir::VarOrImmArg &address, ir::Variable src) {
        auto srcReg32 = compiler.regAlloc.Get(src);
        if (address.immediate) {
            CompileInvokeHostFunction(compiler, fn, m_system, address.imm.value, srcReg32);
        } else {
            auto addrReg32 = compiler.regAlloc.Get(address.var.var);
            CompileInvokeHostFunction(compiler, fn, m_system, addrReg32, srcReg32);
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

void x64Host::CompileOp(Compiler &compiler, const ir::IRPreloadOp *op) {
    // TODO: implement
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLogicalShiftLeftOp *op) {
    auto &codegen = compiler.codegen;
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // x86 masks the shift amount to 31 or 63.
    // ARM does not -- larger amounts simply output zero.
    // For offset == 32, the carry flag is set to bit 0 of the base value.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::LSL(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(compiler, op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg64 = compiler.regAlloc.GetRCX();
        auto valueReg64 = compiler.regAlloc.Get(op->value.var.var).cvt64();
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        codegen.mov(shiftReg64, 63);
        codegen.cmp(amountReg32, 63);
        codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Get destination register
        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            auto dstReg64 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            codegen.shlx(dstReg64, valueReg64, shiftReg64);
        } else {
            Xbyak::Reg64 dstReg{};
            if (op->dst.var.IsPresent()) {
                dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
                CopyIfDifferent(compiler, dstReg, valueReg64);
            } else {
                dstReg = compiler.regAlloc.GetTemporary().cvt64();
            }

            // Compute the shift
            codegen.shl(dstReg, 32); // Shift value to the top half of the 64-bit register
            codegen.shl(dstReg, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
            codegen.shr(dstReg, 32); // Shift value back down to the bottom half
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg64 = compiler.regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        codegen.mov(shiftReg64, 63);
        codegen.cmp(amountReg32, 63);
        codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Get destination register
        Xbyak::Reg64 dstReg64{};
        if (op->dst.var.IsPresent()) {
            dstReg64 = compiler.regAlloc.Get(op->dst.var).cvt64();
        } else {
            dstReg64 = compiler.regAlloc.GetTemporary().cvt64();
        }

        // Compute the shift
        codegen.mov(dstReg64, static_cast<uint64_t>(value) << 32ull);
        codegen.shl(dstReg64, shiftReg64.cvt8());
        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
        codegen.shr(dstReg64, 32); // Shift value back down to the bottom half
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value;

        if (amount < 32) {
            // Get destination register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(compiler, dstReg32, valueReg32);
            } else {
                dstReg32 = compiler.regAlloc.GetTemporary();
            }

            // Compute shift and update flags
            codegen.shl(dstReg32, amount);
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else {
            if (amount == 32) {
                if (op->dst.var.IsPresent()) {
                    // Update carry flag before zeroing out the register
                    if (op->setCarry) {
                        codegen.bt(valueReg32, 0);
                        SetCFromFlags(compiler);
                    }

                    auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    codegen.xor_(dstReg32, dstReg32);
                } else if (op->setCarry) {
                    codegen.bt(valueReg32, 0);
                    SetCFromFlags(compiler);
                }
            } else {
                // Zero out destination
                if (op->dst.var.IsPresent()) {
                    auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    codegen.xor_(dstReg32, dstReg32);
                }
                if (op->setCarry) {
                    SetCFromValue(compiler, false);
                }
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLogicalShiftRightOp *op) {
    auto &codegen = compiler.codegen;
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // x86 masks the shift amount to 31 or 63.
    // ARM does not -- larger amounts simply output zero.
    // For offset == 32, the carry flag is set to bit 31.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::LSR(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(compiler, op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg64 = compiler.regAlloc.GetRCX();
        auto valueReg64 = compiler.regAlloc.Get(op->value.var.var).cvt64();
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        codegen.mov(shiftReg64, 63);
        codegen.cmp(amountReg32, 63);
        codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Compute the shift
        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            auto dstReg64 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            codegen.shrx(dstReg64, valueReg64, shiftReg64);
        } else if (op->dst.var.IsPresent()) {
            auto dstReg64 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            CopyIfDifferent(compiler, dstReg64, valueReg64);
            codegen.shr(dstReg64, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            codegen.dec(shiftReg64);
            codegen.bt(valueReg64, shiftReg64);
            SetCFromFlags(compiler);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg64 = compiler.regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        codegen.mov(shiftReg64, 63);
        codegen.cmp(amountReg32, 63);
        codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = compiler.regAlloc.Get(op->dst.var).cvt64();
            codegen.mov(dstReg64, value);
            codegen.shr(dstReg64, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            auto valueReg64 = compiler.regAlloc.GetTemporary().cvt64();
            codegen.mov(valueReg64, (static_cast<uint64_t>(value) << 1ull));
            codegen.bt(valueReg64, shiftReg64);
            SetCFromFlags(compiler);
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value;

        if (amount < 32) {
            // Compute the shift
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(compiler, dstReg32, valueReg32);
                codegen.shr(dstReg32, amount);
                if (op->setCarry) {
                    SetCFromFlags(compiler);
                }
            } else if (op->setCarry) {
                codegen.bt(valueReg32.cvt64(), amount - 1);
                SetCFromFlags(compiler);
            }
        } else if (amount == 32) {
            if (op->dst.var.IsPresent()) {
                // Update carry flag before zeroing out the register
                if (op->setCarry) {
                    codegen.bt(valueReg32, 31);
                    SetCFromFlags(compiler);
                }

                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                codegen.xor_(dstReg32, dstReg32);
            } else if (op->setCarry) {
                codegen.bt(valueReg32, 31);
                SetCFromFlags(compiler);
            }
        } else {
            // Zero out destination
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                codegen.xor_(dstReg32, dstReg32);
            }
            if (op->setCarry) {
                SetCFromValue(compiler, false);
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRArithmeticShiftRightOp *op) {
    auto &codegen = compiler.codegen;
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // x86 masks the shift amount to 31 or 63.
    // ARM does not, though amounts larger than 31 behave exactly the same as a shift by 31.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::LSR(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(compiler, op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg32 = compiler.regAlloc.GetRCX().cvt32();
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..31
        codegen.mov(shiftReg32, 31);
        codegen.cmp(amountReg32, 31);
        codegen.cmovbe(shiftReg32.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            codegen.sar(dstReg32, shiftReg32.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            codegen.dec(shiftReg32);
            codegen.bt(valueReg32, shiftReg32);
            SetCFromFlags(compiler);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg32 = compiler.regAlloc.GetRCX().cvt32();
        auto value = static_cast<int32_t>(op->value.imm.value);
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..31
        codegen.mov(shiftReg32, 31);
        codegen.cmp(amountReg32, 31);
        codegen.cmovbe(shiftReg32.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            codegen.mov(dstReg32, value);
            codegen.sar(dstReg32, shiftReg32.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            auto valueReg32 = compiler.regAlloc.GetTemporary();
            codegen.mov(valueReg32, (static_cast<uint64_t>(value) << 1ull));
            codegen.bt(valueReg32, shiftReg32);
            SetCFromFlags(compiler);
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amount = std::min(op->amount.imm.value, 31u);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(compiler, dstReg32, valueReg32);
            codegen.sar(dstReg32, amount);
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            codegen.bt(valueReg32.cvt64(), amount - 1);
            SetCFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRRotateRightOp *op) {
    auto &codegen = compiler.codegen;
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // ARM ROR works exactly the same as x86 ROR, including carry flag behavior.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::ROR(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(compiler, op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg8 = compiler.regAlloc.GetRCX().cvt8();
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Put shift amount into CL
        codegen.mov(shiftReg8, amountReg32.cvt8());

        // Put value to shift into the result register
        Xbyak::Reg32 dstReg32{};
        if (op->dst.var.IsPresent()) {
            dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(compiler, dstReg32, valueReg32);
        } else {
            dstReg32 = compiler.regAlloc.GetTemporary();
            codegen.mov(dstReg32, valueReg32);
        }

        // Compute the shift
        codegen.ror(dstReg32, shiftReg8);
        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg8 = compiler.regAlloc.GetRCX().cvt8();
        auto value = op->value.imm.value;
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Put shift amount into CL
        codegen.mov(shiftReg8, amountReg32.cvt8());

        // Put value to shift into the result register
        Xbyak::Reg32 dstReg32{};
        if (op->dst.var.IsPresent()) {
            dstReg32 = compiler.regAlloc.Get(op->dst.var);
        } else if (op->setCarry) {
            dstReg32 = compiler.regAlloc.GetTemporary();
        }

        // Compute the shift
        codegen.mov(dstReg32, value);
        codegen.ror(dstReg32, shiftReg8);
        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value & 31;

        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            // Compute the shift directly into the result register
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            codegen.rorx(dstReg32, valueReg32, amount);
        } else {
            // Put value to shift into the result register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(compiler, dstReg32, valueReg32);
            } else {
                dstReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(dstReg32, valueReg32);
            }

            // Compute the shift
            codegen.ror(dstReg32, amount);
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRRotateRightExtendedOp *op) {
    // ARM RRX works exactly the same as x86 RRX, including carry flag behavior.
    auto &codegen = compiler.codegen;

    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 dstReg32{};

        if (op->value.immediate) {
            dstReg32 = compiler.regAlloc.Get(op->dst.var);
            codegen.mov(dstReg32, op->value.imm.value);
        } else {
            auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
            dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(compiler, dstReg32, valueReg32);
        }

        codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Refresh carry flag
        codegen.rcr(dstReg32, 1);                   // Perform RRX

        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
    } else if (op->setCarry) {
        if (op->value.immediate) {
            SetCFromValue(compiler, bit::test<0>(op->value.imm.value));
        } else {
            auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
            codegen.bt(valueReg32, 0);
            SetCFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseAndOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value & op->rhs.imm.value;
        AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(compiler, dstReg32, varReg32);
                codegen.and_(dstReg32, imm);
            } else if (setFlags) {
                codegen.test(varReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

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
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseOrOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value | op->rhs.imm.value;
        AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(compiler, dstReg32, varReg32);
                codegen.or_(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.or_(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    codegen.or_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    codegen.or_(dstReg32, lhsReg32);
                } else {
                    codegen.mov(dstReg32, lhsReg32);
                    codegen.or_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                codegen.mov(tmpReg32, lhsReg32);
                codegen.or_(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseXorOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value ^ op->rhs.imm.value;
        AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(compiler, dstReg32, varReg32);
                codegen.xor_(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.xor_(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    codegen.xor_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    codegen.xor_(dstReg32, lhsReg32);
                } else {
                    codegen.mov(dstReg32, lhsReg32);
                    codegen.xor_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                codegen.mov(tmpReg32, lhsReg32);
                codegen.xor_(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitClearOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value & ~op->rhs.imm.value;
        AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (!rhsImm) {
            // lhs is var or imm, rhs is variable
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->rhs.var.var);
                CopyIfDifferent(compiler, dstReg32, rhsReg32);
                codegen.not_(dstReg32);

                if (lhsImm) {
                    codegen.and_(dstReg32, op->lhs.imm.value);
                } else {
                    auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
                    codegen.and_(dstReg32, lhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(tmpReg32, rhsReg32);
                codegen.not_(tmpReg32);

                if (lhsImm) {
                    codegen.test(tmpReg32, op->lhs.imm.value);
                } else {
                    auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
                    codegen.test(tmpReg32, lhsReg32);
                }
            }
        } else {
            // lhs is variable, rhs is immediate
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(compiler, dstReg32, lhsReg32);
                codegen.and_(dstReg32, ~op->rhs.imm.value);
            } else if (setFlags) {
                codegen.test(lhsReg32, ~op->rhs.imm.value);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRCountLeadingZerosOp *op) {
    auto &codegen = compiler.codegen;
    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 valReg32{};
        Xbyak::Reg32 dstReg32{};
        if (op->value.immediate) {
            valReg32 = compiler.regAlloc.GetTemporary();
            dstReg32 = compiler.regAlloc.Get(op->dst.var);
            codegen.mov(valReg32, op->value.imm.value);
        } else {
            valReg32 = compiler.regAlloc.Get(op->value.var.var);
            dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(compiler, dstReg32, valReg32);
        }

        if (CPUID::HasLZCNT()) {
            codegen.lzcnt(dstReg32, valReg32);
        } else {
            // BSR unhelpfully returns the bit offset from the right, not left
            auto valIfZero32 = compiler.regAlloc.GetTemporary();
            codegen.mov(valIfZero32, 0xFFFFFFFF);
            codegen.bsr(dstReg32, valReg32);
            codegen.cmovz(dstReg32, valIfZero32);
            codegen.neg(dstReg32);
            codegen.add(dstReg32, 31);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        auto [result, carry, overflow] = arm::ADD(op->lhs.imm.value, op->rhs.imm.value);
        AssignImmResultWithNZCV(compiler, op->dst, result, carry, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(compiler, dstReg32, varReg32);
                codegen.add(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.add(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    codegen.mov(dstReg32, lhsReg32);
                }
                codegen.add(dstReg32, op2Reg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                codegen.mov(tmpReg32, lhsReg32);
                codegen.add(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddCarryOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            codegen.mov(dstReg32, op->lhs.imm.value);

            codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            codegen.adc(dstReg32, op->rhs.imm.value);
            if (setFlags) {
                SetNZCVFromFlags(compiler);
            }
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(compiler, dstReg32, varReg32);
                codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                codegen.adc(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                codegen.adc(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    codegen.mov(dstReg32, lhsReg32);
                }
                codegen.adc(dstReg32, op2Reg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                codegen.mov(tmpReg32, lhsReg32);
                codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                codegen.adc(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSubtractOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        auto [result, carry, overflow] = arm::SUB(op->lhs.imm.value, op->rhs.imm.value);
        AssignImmResultWithNZCV(compiler, op->dst, result, carry, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (!lhsImm) {
            // lhs is variable, rhs is var or imm
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(compiler, dstReg32, lhsReg32);

                if (rhsImm) {
                    codegen.sub(dstReg32, op->rhs.imm.value);
                } else {
                    auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
                    codegen.sub(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                if (rhsImm) {
                    codegen.cmp(lhsReg32, op->rhs.imm.value);
                } else {
                    auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
                    codegen.cmp(lhsReg32, rhsReg32);
                }
            }
        } else {
            // lhs is immediate, rhs is variable
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
                codegen.mov(dstReg32, op->lhs.imm.value);
                codegen.sub(dstReg32, rhsReg32);
            } else if (setFlags) {
                auto lhsReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(lhsReg32, op->lhs.imm.value);
                codegen.cmp(lhsReg32, rhsReg32);
            }
        }

        if (setFlags) {
            codegen.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
            SetNZCVFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSubtractCarryOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    // Note: x86 and ARM have inverted borrow bits

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            codegen.mov(dstReg32, op->lhs.imm.value);

            codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            codegen.cmc();                              // Complement it
            codegen.sbb(dstReg32, op->rhs.imm.value);
            if (setFlags) {
                codegen.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
                SetNZCVFromFlags(compiler);
            }
        }
    } else {
        // At least one of the operands is a variable
        if (!lhsImm) {
            // lhs is variable, rhs is var or imm
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);

            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(compiler, dstReg32, lhsReg32);
            } else {
                dstReg32 = compiler.regAlloc.GetTemporary();
            }

            codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            codegen.cmc();                              // Complement it
            if (rhsImm) {
                codegen.sbb(dstReg32, op->rhs.imm.value);
            } else {
                auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
                codegen.sbb(dstReg32, rhsReg32);
            }
        } else {
            // lhs is immediate, rhs is variable
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = compiler.regAlloc.Get(op->dst.var);
            } else {
                dstReg32 = compiler.regAlloc.GetTemporary();
            }
            codegen.mov(dstReg32, op->lhs.imm.value);
            codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            codegen.cmc();                              // Complement it
            codegen.sbb(dstReg32, rhsReg32);
        }

        if (setFlags) {
            codegen.cmc(); // Complement carry output
            SetNZCVFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMoveOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            MOVImmediate(compiler, dstReg32, op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(compiler, dstReg32, valReg32);
        }

        if (setFlags) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            SetNZFromReg(compiler, dstReg32);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(compiler, op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            SetNZFromReg(compiler, valReg32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMoveNegatedOp *op) {
    auto &codegen = compiler.codegen;
    const bool setFlags = BitmaskEnum(op->flags).Any();
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            MOVImmediate(compiler, dstReg32, ~op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);

            CopyIfDifferent(compiler, dstReg32, valReg32);
            codegen.not_(dstReg32);
        }

        if (setFlags) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            SetNZFromReg(compiler, dstReg32);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(compiler, ~op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            auto tmpReg32 = compiler.regAlloc.GetTemporary();
            codegen.mov(tmpReg32, valReg32);
            codegen.not_(tmpReg32);
            SetNZFromReg(compiler, tmpReg32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSaturatingAddOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const int64_t lhsVal = bit::sign_extend<32, int64_t>(op->lhs.imm.value);
        const int64_t rhsVal = bit::sign_extend<32, int64_t>(op->rhs.imm.value);
        const auto [result, overflow] = arm::Saturate(lhsVal + rhsVal);
        AssignImmResultWithOverflow(compiler, op->dst, result, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(compiler, dstReg32, varReg32);
                codegen.add(dstReg32, imm);
                if (setFlags) {
                    SetVFromFlags(compiler);
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(maxValReg32, std::numeric_limits<int32_t>::max());
                codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.add(tmpReg32, imm);
                SetVFromFlags(compiler);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    codegen.add(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    codegen.add(dstReg32, lhsReg32);
                } else {
                    codegen.mov(dstReg32, lhsReg32);
                    codegen.add(dstReg32, rhsReg32);
                }

                if (setFlags) {
                    SetVFromFlags(compiler);
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(maxValReg32, std::numeric_limits<int32_t>::max());
                codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(tmpReg32, lhsReg32);
                codegen.add(tmpReg32, rhsReg32);
                SetVFromFlags(compiler);
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSaturatingSubtractOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const int64_t lhsVal = bit::sign_extend<32, int64_t>(op->lhs.imm.value);
        const int64_t rhsVal = bit::sign_extend<32, int64_t>(op->rhs.imm.value);
        const auto [result, overflow] = arm::Saturate(lhsVal - rhsVal);
        AssignImmResultWithOverflow(compiler, op->dst, result, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(compiler, dstReg32, varReg32);
                codegen.sub(dstReg32, imm);
                if (setFlags) {
                    SetVFromFlags(compiler);
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(maxValReg32, std::numeric_limits<int32_t>::min());
                codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(tmpReg32, varReg32);
                codegen.sub(tmpReg32, imm);
                SetVFromFlags(compiler);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    codegen.sub(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    codegen.sub(dstReg32, lhsReg32);
                } else {
                    codegen.mov(dstReg32, lhsReg32);
                    codegen.sub(dstReg32, rhsReg32);
                }

                if (setFlags) {
                    SetVFromFlags(compiler);
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(maxValReg32, std::numeric_limits<int32_t>::min());
                codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                codegen.mov(tmpReg32, lhsReg32);
                codegen.sub(tmpReg32, rhsReg32);
                SetVFromFlags(compiler);
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMultiplyOp *op) {
    auto &codegen = compiler.codegen;
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->signedMul) {
            auto result = static_cast<int32_t>(op->lhs.imm.value) * static_cast<int32_t>(op->rhs.imm.value);
            AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
        } else {
            auto result = op->lhs.imm.value * op->rhs.imm.value;
            AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(compiler, dstReg32, varReg32);
                if (op->signedMul) {
                    codegen.imul(dstReg32, dstReg32, static_cast<int32_t>(imm));
                } else {
                    codegen.imul(dstReg32.cvt64(), dstReg32.cvt64(), imm);
                }
                if (setFlags) {
                    codegen.test(dstReg32, dstReg32); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
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
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

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
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

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
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMultiplyLongOp *op) {
    auto &codegen = compiler.codegen;
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
            AssignLongImmResultWithNZ(compiler, op->dstLo, op->dstHi, result, setFlags);
        } else {
            auto result = static_cast<uint64_t>(op->lhs.imm.value) * static_cast<uint64_t>(op->rhs.imm.value);
            if (op->shiftDownHalf) {
                result >>= 16ull;
            }
            AssignLongImmResultWithNZ(compiler, op->dstLo, op->dstHi, result, setFlags);
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dstLo.var.IsPresent() || op->dstHi.var.IsPresent()) {
                // Use dstLo or a temporary register for the 64-bit multiplication
                Xbyak::Reg64 dstReg64{};
                if (op->dstLo.var.IsPresent()) {
                    dstReg64 = compiler.regAlloc.ReuseAndGet(op->dstLo.var, var).cvt64();
                } else {
                    dstReg64 = compiler.regAlloc.GetTemporary().cvt64();
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
                    auto dstHiReg64 = compiler.regAlloc.Get(op->dstHi.var).cvt64();
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
                auto tmpReg64 = compiler.regAlloc.GetTemporary().cvt64();
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
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dstLo.var.IsPresent() || op->dstHi.var.IsPresent()) {
                // Use dstLo or a temporary register for the 64-bit multiplication
                Xbyak::Reg64 dstReg64{};
                if (op->dstLo.var.IsPresent()) {
                    compiler.regAlloc.Reuse(op->dstLo.var, op->lhs.var.var);
                    compiler.regAlloc.Reuse(op->dstLo.var, op->rhs.var.var);
                    dstReg64 = compiler.regAlloc.Get(op->dstLo.var).cvt64();
                } else {
                    dstReg64 = compiler.regAlloc.GetTemporary().cvt64();
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
                    auto dstHiReg64 = compiler.regAlloc.Get(op->dstHi.var).cvt64();
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
                auto tmpReg64 = compiler.regAlloc.GetTemporary().cvt64();
                auto op2Reg64 = compiler.regAlloc.GetTemporary().cvt64();

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
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddLongOp *op) {
    auto &codegen = compiler.codegen;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    // Contains the value 32 to be used in shifts
    Xbyak::Reg64 shiftBy32Reg64{};
    if (CPUID::HasBMI2()) {
        shiftBy32Reg64 = compiler.regAlloc.GetTemporary().cvt64();
        codegen.mov(shiftBy32Reg64, 32);
    }

    // Compose two input variables (lo and hi) into a single 64-bit register
    auto compose64 = [&](const ir::VarOrImmArg &lo, const ir::VarOrImmArg &hi) {
        auto outReg64 = compiler.regAlloc.GetTemporary().cvt64();
        if (lo.immediate && hi.immediate) {
            // Both are immediates
            const uint64_t value = static_cast<uint64_t>(lo.imm.value) | (static_cast<uint64_t>(hi.imm.value) << 32ull);
            codegen.mov(outReg64, value);
        } else if (!lo.immediate && !hi.immediate) {
            // Both are variables
            auto loReg64 = compiler.regAlloc.Get(lo.var.var).cvt64();
            auto hiReg64 = compiler.regAlloc.Get(hi.var.var).cvt64();

            if (CPUID::HasBMI2()) {
                codegen.shlx(outReg64, hiReg64, shiftBy32Reg64);
            } else {
                codegen.mov(outReg64, hiReg64);
                codegen.shl(outReg64, 32);
            }
            codegen.or_(outReg64, loReg64);
        } else if (lo.immediate) {
            // lo is immediate, hi is variable
            auto hiReg64 = compiler.regAlloc.Get(hi.var.var).cvt64();

            if (outReg64 != hiReg64 && CPUID::HasBMI2()) {
                codegen.shlx(outReg64, hiReg64, shiftBy32Reg64);
            } else {
                CopyIfDifferent(compiler, outReg64, hiReg64);
                codegen.shl(outReg64, 32);
            }
            codegen.or_(outReg64, lo.imm.value);
        } else {
            // lo is variable, hi is immediate
            auto loReg64 = compiler.regAlloc.Get(lo.var.var).cvt64();

            if (outReg64 != loReg64 && CPUID::HasBMI2()) {
                codegen.shlx(outReg64, loReg64, shiftBy32Reg64);
            } else {
                CopyIfDifferent(compiler, outReg64, loReg64);
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
    if (op->dstLo.var.IsPresent() && compiler.regAlloc.AssignTemporary(op->dstLo.var, lhsReg64.cvt32())) {
        // Assign one of the temporary variables to dstLo
        dstLoReg64 = compiler.regAlloc.Get(op->dstLo.var).cvt64();
    } else {
        // Create a new temporary variable if dstLo is absent or the temporary register assignment failed
        dstLoReg64 = compiler.regAlloc.GetTemporary().cvt64();
        codegen.mov(dstLoReg64, lhsReg64);
    }
    codegen.add(dstLoReg64, rhsReg64);

    // Update flags if requested
    if (setFlags) {
        SetNZFromFlags(compiler);
    }

    // Put top half of the result into dstHi if it is present
    if (op->dstHi.var.IsPresent()) {
        auto dstHiReg64 = compiler.regAlloc.Get(op->dstHi.var).cvt64();
        if (CPUID::HasBMI2()) {
            codegen.shrx(dstHiReg64, dstLoReg64, shiftBy32Reg64);
        } else {
            codegen.mov(dstHiReg64, dstLoReg64);
            codegen.shr(dstHiReg64, 32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRStoreFlagsOp *op) {
    auto &codegen = compiler.codegen;
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
            auto valReg32 = compiler.regAlloc.Get(op->values.var.var);
            auto maskReg32 = compiler.regAlloc.GetTemporary();
            codegen.shr(valReg32, ARMflgNZCVShift);
            codegen.imul(valReg32, valReg32, ARMTox64FlagsMult);
            codegen.and_(valReg32, x64FlagsMask);
            codegen.mov(maskReg32, (~mask * ARMTox64FlagsMult) & x64FlagsMask);
            codegen.and_(abi::kHostFlagsReg, maskReg32);
            codegen.or_(abi::kHostFlagsReg, valReg32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadFlagsOp *op) {
    auto &codegen = compiler.codegen;
    // Get value from srcCPSR and copy to dstCPSR, or reuse register from srcCPSR if possible
    if (op->srcCPSR.immediate) {
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);
        codegen.mov(dstReg32, op->srcCPSR.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(compiler, dstReg32, srcReg32);
    }

    // Apply flags to dstReg32
    if (BitmaskEnum(op->flags).Any()) {
        const uint32_t cpsrMask = static_cast<uint32_t>(op->flags);
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);

        // Extract the host flags we need from EAX into flags
        auto flags = compiler.regAlloc.GetTemporary();
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

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadStickyOverflowOp *op) {
    auto &codegen = compiler.codegen;
    if (op->setQ) {
        auto srcReg32 = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);

        // Apply overflow flag in the Q position
        codegen.mov(dstReg32, abi::kHostFlagsReg.cvt8()); // Copy overflow flag into destination register
        codegen.shl(dstReg32, ARMflgQPos);                // Move Q into position
        codegen.or_(dstReg32, srcReg32);                  // OR with srcCPSR
    } else if (op->srcCPSR.immediate) {
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);
        codegen.mov(dstReg32, op->srcCPSR.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(compiler, dstReg32, srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBranchOp *op) {
    auto &codegen = compiler.codegen;
    const auto pcFieldOffset = m_armState.GPROffset(arm::GPR::PC, compiler.mode);
    const auto pcOffset = 2 * (compiler.thumb ? sizeof(uint16_t) : sizeof(uint32_t));
    const auto addrMask = (compiler.thumb ? ~1 : ~3);

    if (op->address.immediate) {
        codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], (op->address.imm.value & addrMask) + pcOffset);
    } else {
        auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
        auto tmpReg32 = compiler.regAlloc.GetTemporary();
        codegen.lea(tmpReg32, dword[addrReg32 + pcOffset]);
        codegen.and_(tmpReg32, addrMask);
        codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], tmpReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBranchExchangeOp *op) {
    auto &codegen = compiler.codegen;
    Xbyak::Label lblEnd;
    Xbyak::Label lblExchange;

    auto pcReg32 = compiler.regAlloc.GetTemporary();
    const auto pcFieldOffset = m_armState.GPROffset(arm::GPR::PC, compiler.mode);
    const auto cpsrFieldOffset = m_armState.CPSROffset();

    // Honor pre-ARMv5 branching feature if requested
    if (op->bx4) {
        auto &cp15 = m_context.GetARMState().GetSystemControlCoprocessor();
        if (cp15.IsPresent()) {
            auto &cp15ctl = cp15.GetControlRegister();
            const auto ctlValueOfs = offsetof(arm::cp15::ControlRegister, value);

            // Use pcReg32 as scratch register for this test
            codegen.mov(pcReg32.cvt64(), CastUintPtr(&cp15ctl));
            codegen.test(dword[pcReg32.cvt64() + ctlValueOfs], (1 << 15)); // L4 bit
            codegen.je(lblExchange);

            // Perform branch without exchange
            const auto pcOffset = 2 * (compiler.thumb ? sizeof(uint16_t) : sizeof(uint32_t));
            const auto addrMask = (compiler.thumb ? ~1 : ~3);
            if (op->address.immediate) {
                codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], (op->address.imm.value & addrMask) + pcOffset);
            } else {
                auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
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

        auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);

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

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadCopRegisterOp *op) {
    if (!op->dstValue.var.IsPresent()) {
        return;
    }

    auto func = (op->ext) ? SystemLoadCopExtRegister : SystemLoadCopRegister;
    auto dstReg32 = compiler.regAlloc.Get(op->dstValue.var);
    CompileInvokeHostFunction(compiler, dstReg32, func, m_armState, op->cpnum, op->reg.u16);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRStoreCopRegisterOp *op) {
    auto func = (op->ext) ? SystemStoreCopExtRegister : SystemStoreCopRegister;
    if (op->srcValue.immediate) {
        CompileInvokeHostFunction(compiler, func, m_armState, op->cpnum, op->reg.u16, op->srcValue.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->srcValue.var.var);
        CompileInvokeHostFunction(compiler, func, m_armState, op->cpnum, op->reg.u16, srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRConstantOp *op) {
    // This instruction should be optimized away, but here's an implementation anyway
    auto &codegen = compiler.codegen;
    if (op->dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
        codegen.mov(dstReg32, op->value);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRCopyVarOp *op) {
    // This instruction should be optimized away, but here's an implementation anyway
    if (!op->var.var.IsPresent() || !op->dst.var.IsPresent()) {
        return;
    }
    auto varReg32 = compiler.regAlloc.Get(op->var.var);
    auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->var.var);
    CopyIfDifferent(compiler, dstReg32, varReg32);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetBaseVectorAddressOp *op) {
    auto &codegen = compiler.codegen;
    if (op->dst.var.IsPresent()) {
        auto &cp15 = m_armState.GetSystemControlCoprocessor();
        if (cp15.IsPresent()) {
            // Load base vector address from CP15
            auto &cp15ctl = cp15.GetControlRegister();
            const auto baseVectorAddressOfs = offsetof(arm::cp15::ControlRegister, baseVectorAddress);

            auto ctlPtrReg64 = compiler.regAlloc.GetTemporary().cvt64();
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            codegen.mov(ctlPtrReg64, CastUintPtr(&cp15ctl));
            codegen.mov(dstReg32, dword[ctlPtrReg64 + baseVectorAddressOfs]);
        } else {
            // Default to 00000000 if CP15 is absent
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            codegen.xor_(dstReg32, dstReg32);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void x64Host::SetCFromValue(Compiler &compiler, bool carry) {
    auto &codegen = compiler.codegen;
    if (carry) {
        codegen.or_(abi::kHostFlagsReg, x64flgC);
    } else {
        codegen.and_(abi::kHostFlagsReg, ~x64flgC);
    }
}

void x64Host::SetCFromFlags(Compiler &compiler) {
    auto &codegen = compiler.codegen;
    auto tmp32 = compiler.regAlloc.GetTemporary();
    codegen.setc(tmp32.cvt8());                 // Put new C into a temporary register
    codegen.movzx(tmp32, tmp32.cvt8());         // Zero-extend to 32 bits
    codegen.shl(tmp32, x64flgCPos);             // Move it to the correct position
    codegen.and_(abi::kHostFlagsReg, ~x64flgC); // Clear existing C flag from EAX
    codegen.or_(abi::kHostFlagsReg, tmp32);     // Write new C flag into EAX
}

void x64Host::SetVFromValue(Compiler &compiler, bool overflow) {
    auto &codegen = compiler.codegen;
    if (overflow) {
        codegen.mov(abi::kHostFlagsReg.cvt8(), 1);
    } else {
        codegen.xor_(abi::kHostFlagsReg.cvt8(), abi::kHostFlagsReg.cvt8());
    }
}

void x64Host::SetVFromFlags(Compiler &compiler) {
    auto &codegen = compiler.codegen;
    codegen.seto(abi::kHostFlagsReg.cvt8());
}

void x64Host::SetNZFromValue(Compiler &compiler, uint32_t value) {
    auto &codegen = compiler.codegen;
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

void x64Host::SetNZFromValue(Compiler &compiler, uint64_t value) {
    auto &codegen = compiler.codegen;
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

void x64Host::SetNZFromReg(Compiler &compiler, Xbyak::Reg32 value) {
    auto &codegen = compiler.codegen;
    auto tmp32 = compiler.regAlloc.GetTemporary();
    codegen.test(value, value);             // Updates NZ, clears CV; V won't be changed here
    codegen.mov(tmp32, abi::kHostFlagsReg); // Copy current flags to preserve C later
    codegen.lahf();                         // Load NZC; C is 0
    codegen.and_(tmp32, x64flgC);           // Keep previous C only
    codegen.or_(abi::kHostFlagsReg, tmp32); // Put previous C into AH; NZ is now updated and C is preserved
}

void x64Host::SetNZFromFlags(Compiler &compiler) {
    auto &codegen = compiler.codegen;
    auto tmp32 = compiler.regAlloc.GetTemporary();
    codegen.clc();                          // Clear C to make way for the previous C
    codegen.mov(tmp32, abi::kHostFlagsReg); // Copy current flags to preserve C later
    codegen.lahf();                         // Load NZC; C is 0
    codegen.and_(tmp32, x64flgC);           // Keep previous C only
    codegen.or_(abi::kHostFlagsReg, tmp32); // Put previous C into AH; NZ is now updated and C is preserved
}

void x64Host::SetNZCVFromValue(Compiler &compiler, uint32_t value, bool carry, bool overflow) {
    auto &codegen = compiler.codegen;
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

void x64Host::SetNZCVFromFlags(Compiler &compiler) {
    auto &codegen = compiler.codegen;
    codegen.lahf();
    codegen.seto(abi::kHostFlagsReg.cvt8());
}

void x64Host::MOVImmediate(Compiler &compiler, Xbyak::Reg32 reg, uint32_t value) {
    auto &codegen = compiler.codegen;
    if (value == 0) {
        codegen.xor_(reg, reg);
    } else {
        codegen.mov(reg, value);
    }
}

void x64Host::CopyIfDifferent(Compiler &compiler, Xbyak::Reg32 dst, Xbyak::Reg32 src) {
    auto &codegen = compiler.codegen;
    if (dst != src) {
        codegen.mov(dst, src);
    }
}

void x64Host::CopyIfDifferent(Compiler &compiler, Xbyak::Reg64 dst, Xbyak::Reg64 src) {
    auto &codegen = compiler.codegen;
    if (dst != src) {
        codegen.mov(dst, src);
    }
}

void x64Host::AssignImmResultWithNZ(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(compiler, dstReg32, result);
    }

    if (setFlags) {
        SetNZFromValue(compiler, result);
    }
}

void x64Host::AssignImmResultWithNZCV(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool carry,
                                      bool overflow, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(compiler, dstReg32, result);
    }

    if (setFlags) {
        SetNZCVFromValue(compiler, result, carry, overflow);
    }
}

void x64Host::AssignImmResultWithCarry(Compiler &compiler, const ir::VariableArg &dst, uint32_t result,
                                       std::optional<bool> carry, bool setCarry) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(compiler, dstReg32, result);
    }

    if (setCarry && carry) {
        SetCFromValue(compiler, *carry);
    }
}

void x64Host::AssignImmResultWithOverflow(Compiler &compiler, const ir::VariableArg &dst, uint32_t result,
                                          bool overflow, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(compiler, dstReg32, result);
    }

    if (setFlags) {
        SetVFromValue(compiler, overflow);
    }
}

void x64Host::AssignLongImmResultWithNZ(Compiler &compiler, const ir::VariableArg &dstLo, const ir::VariableArg &dstHi,
                                        uint64_t result, bool setFlags) {
    if (dstLo.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dstLo.var);
        MOVImmediate(compiler, dstReg32, result);
    }
    if (dstHi.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dstHi.var);
        MOVImmediate(compiler, dstReg32, result >> 32ull);
    }

    if (setFlags) {
        SetNZFromValue(compiler, result);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
constexpr bool is_raw_integral_v = std::is_integral_v<std::remove_cvref_t<T>>;

template <typename Base, typename Derived>
constexpr bool is_raw_base_of_v = std::is_base_of_v<Base, std::remove_cvref_t<Derived>>;

template <typename ReturnType, typename... FnArgs, typename... Args>
void x64Host::CompileInvokeHostFunctionImpl(Compiler &compiler, Xbyak::Reg dstReg, ReturnType (*fn)(FnArgs...),
                                            Args &&...args) {
    static_assert(is_raw_integral_v<ReturnType> || std::is_void_v<ReturnType> || std::is_pointer_v<ReturnType> ||
                      std::is_reference_v<ReturnType>,
                  "ReturnType must be an integral type, void, pointer or reference");

    static_assert(((is_raw_integral_v<FnArgs> || is_raw_base_of_v<Xbyak::Operand, FnArgs> ||
                    std::is_pointer_v<FnArgs> || std::is_reference_v<FnArgs>)&&...),
                  "All FnArgs must be integral types, Xbyak operands, pointers or references");

    auto &codegen = compiler.codegen;

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
        if (compiler.regAlloc.IsRegisterAllocated(reg)) {
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
            // TODO: push onto stack in the order specified by the ABI
            // abi::kStackGrowsDownward
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
