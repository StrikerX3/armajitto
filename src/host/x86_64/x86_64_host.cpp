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
    Compiler compiler{m_codegen};

    compiler.Analyze(block);

    auto fnPtr = m_codegen.getCurr<HostCode>();
    m_codegen.setProtectModeRW();

    Xbyak::Label lblCondFail{};
    Xbyak::Label lblCondPass{};

    CompileCondCheck(block.Condition(), lblCondFail);

    // Compile block code
    auto *op = block.Head();
    while (op != nullptr) {
        compiler.PreProcessOp(op);
        ir::VisitIROp(op, [this, &compiler](const auto *op) -> void { CompileOp(compiler, op); });
        compiler.PostProcessOp(op);
        op = op->Next();
    }

    // Skip over condition fail block
    m_codegen.jmp(lblCondPass);

    // Update PC if condition fails
    m_codegen.L(lblCondFail);
    const auto pcRegOffset = m_armState.GPROffset(arm::GPR::PC, compiler.mode);
    const auto instrSize = block.Location().IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t);
    m_codegen.mov(dword[abi::kARMStateReg + pcRegOffset], block.Location().PC() + block.InstructionCount() * instrSize);
    // TODO: increment cycles for failing the check

    m_codegen.L(lblCondPass);
    // TODO: fast block linking (pointer in BasicBlock)
    CompileBlockCacheLookup(compiler);
    // TODO: cycle counting

    // Go to epilog
    m_codegen.mov(abi::kNonvolatileRegs[0], m_epilog);
    m_codegen.jmp(abi::kNonvolatileRegs[0]);

    m_codegen.setProtectModeRE();
    m_blockCache.insert({block.Location().ToUint64(), {.code = fnPtr}});

    vtune::ReportBasicBlock(fnPtr, m_codegen.getCurr<uintptr_t>(), block.Location());
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

void x64Host::CompileCondCheck(arm::Condition cond, Xbyak::Label &lblCondFail) {
    switch (cond) {
    case arm::Condition::EQ: // Z=1
        m_codegen.sahf();
        m_codegen.jnz(lblCondFail);
        break;
    case arm::Condition::NE: // Z=0
        m_codegen.sahf();
        m_codegen.jz(lblCondFail);
        break;
    case arm::Condition::CS: // C=1
        m_codegen.sahf();
        m_codegen.jnc(lblCondFail);
        break;
    case arm::Condition::CC: // C=0
        m_codegen.sahf();
        m_codegen.jc(lblCondFail);
        break;
    case arm::Condition::MI: // N=1
        m_codegen.sahf();
        m_codegen.jns(lblCondFail);
        break;
    case arm::Condition::PL: // N=0
        m_codegen.sahf();
        m_codegen.js(lblCondFail);
        break;
    case arm::Condition::VS: // V=1
        m_codegen.cmp(al, 0x81);
        m_codegen.jno(lblCondFail);
        break;
    case arm::Condition::VC: // V=0
        m_codegen.cmp(al, 0x81);
        m_codegen.jo(lblCondFail);
        break;
    case arm::Condition::HI: // C=1 && Z=0
        m_codegen.sahf();
        m_codegen.cmc();
        m_codegen.jna(lblCondFail);
        break;
    case arm::Condition::LS: // C=0 || Z=1
        m_codegen.sahf();
        m_codegen.cmc();
        m_codegen.ja(lblCondFail);
        break;
    case arm::Condition::GE: // N=V
        m_codegen.cmp(al, 0x81);
        m_codegen.sahf();
        m_codegen.jnge(lblCondFail);
        break;
    case arm::Condition::LT: // N!=V
        m_codegen.cmp(al, 0x81);
        m_codegen.sahf();
        m_codegen.jge(lblCondFail);
        break;
    case arm::Condition::GT: // Z=0 && N=V
        m_codegen.cmp(al, 0x81);
        m_codegen.sahf();
        m_codegen.jng(lblCondFail);
        break;
    case arm::Condition::LE: // Z=1 || N!=V
        m_codegen.cmp(al, 0x81);
        m_codegen.sahf();
        m_codegen.jg(lblCondFail);
        break;
    case arm::Condition::AL: // always
        break;
    case arm::Condition::NV: // never
        m_codegen.jmp(lblCondFail);
        break;
    }
}

void x64Host::CompileBlockCacheLookup(Compiler &compiler) {
    Xbyak::Label noEntry{};

    const auto cpsrOffset = m_armState.CPSROffset();
    const auto pcRegOffset = m_armState.GPROffset(arm::GPR::PC, compiler.mode);

    // Build cache key
    auto cacheKeyReg64 = compiler.regAlloc.GetTemporary().cvt64();
    m_codegen.mov(cacheKeyReg64, dword[abi::kARMStateReg + cpsrOffset]);
    m_codegen.shl(cacheKeyReg64.cvt64(), 32);
    m_codegen.or_(cacheKeyReg64, dword[abi::kARMStateReg + pcRegOffset]);

    // Lookup entry
    // TODO: redesign cache to not rely on this function call
    CompileInvokeHostFunction(compiler, cacheKeyReg64, GetCodeForLocation, m_blockCache, cacheKeyReg64);

    // Check for nullptr
    auto cacheEntryReg64 = cacheKeyReg64;
    m_codegen.test(cacheEntryReg64, cacheKeyReg64);
    m_codegen.jz(noEntry);

    // Entry found, jump to linked block
    m_codegen.jmp(cacheEntryReg64);

    // Entry not found, bail out
    m_codegen.L(noEntry);

    // Cleanup temporaries used here
    compiler.regAlloc.ReleaseTemporaries();
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetRegisterOp *op) {
    auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.GPROffset(op->src.gpr, op->src.Mode());
    m_codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetRegisterOp *op) {
    auto offset = m_armState.GPROffset(op->dst.gpr, op->dst.Mode());
    if (op->src.immediate) {
        m_codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
        m_codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetCPSROp *op) {
    auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.CPSROffset();
    m_codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetCPSROp *op) {
    auto offset = m_armState.CPSROffset();
    if (op->src.immediate) {
        m_codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
        m_codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetSPSROp *op) {
    auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.SPSROffset(op->mode);
    m_codegen.mov(dstReg32, dword[abi::kARMStateReg + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetSPSROp *op) {
    auto offset = m_armState.SPSROffset(op->mode);
    if (op->src.immediate) {
        m_codegen.mov(dword[abi::kARMStateReg + offset], op->src.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
        m_codegen.mov(dword[abi::kARMStateReg + offset], srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMemReadOp *op) {
    // TODO: handle caches, permissions, etc.
    // TODO: fast memory LUT, including TCM blocks; replace the TCM checks below
    // TODO: virtual memory, exception handling, rewriting accessors

    Xbyak::Label lblSkipTCM{};
    Xbyak::Label lblEnd{};

    auto compileRead = [this, op, &compiler](Xbyak::Reg32 dstReg32, Xbyak::Reg64 addrReg64) {
        switch (op->size) {
        case ir::MemAccessSize::Byte:
            if (op->mode == ir::MemAccessMode::Signed) {
                m_codegen.movsx(dstReg32, byte[addrReg64]);
            } else { // aligned/unaligned
                m_codegen.movzx(dstReg32, byte[addrReg64]);
            }
            break;
        case ir::MemAccessSize::Half:
            if (op->mode == ir::MemAccessMode::Signed) {
                if (m_context.GetCPUArch() == CPUArch::ARMv4T) {
                    if (op->address.immediate) {
                        if (op->address.imm.value & 1) {
                            m_codegen.movsx(dstReg32, byte[addrReg64 + 1]);
                        } else {
                            m_codegen.movsx(dstReg32, word[addrReg64]);
                        }
                    } else {
                        Xbyak::Label lblByteRead{};
                        Xbyak::Label lblDone{};

                        auto baseAddrReg32 = compiler.regAlloc.Get(op->address.var.var);
                        m_codegen.test(baseAddrReg32, 1);
                        m_codegen.jnz(lblByteRead);

                        // Word read
                        m_codegen.movsx(dstReg32, word[addrReg64]);
                        m_codegen.jmp(lblDone);

                        // Byte read
                        m_codegen.L(lblByteRead);
                        m_codegen.movsx(dstReg32, byte[addrReg64 + 1]);

                        m_codegen.L(lblDone);
                    }
                } else {
                    m_codegen.movsx(dstReg32, word[addrReg64]);
                }
            } else if (op->mode == ir::MemAccessMode::Unaligned) {
                m_codegen.movzx(dstReg32, word[addrReg64]);
                if (m_context.GetCPUArch() == CPUArch::ARMv4T) {
                    if (op->address.immediate) {
                        const uint32_t shiftOffset = (op->address.imm.value & 1) * 8;
                        if (shiftOffset != 0) {
                            m_codegen.ror(dstReg32, shiftOffset);
                        }
                    } else {
                        auto baseAddrReg32 = compiler.regAlloc.Get(op->address.var.var);
                        auto shiftReg32 = compiler.regAlloc.GetRCX().cvt32();
                        m_codegen.mov(shiftReg32, baseAddrReg32);
                        m_codegen.and_(shiftReg32, 1);
                        m_codegen.shl(shiftReg32, 3);
                        m_codegen.ror(dstReg32, shiftReg32.cvt8());
                    }
                }
            } else { // aligned
                m_codegen.movzx(dstReg32, word[addrReg64]);
            }
            break;
        case ir::MemAccessSize::Word:
            m_codegen.mov(dstReg32, dword[addrReg64]);
            if (op->mode == ir::MemAccessMode::Unaligned) {
                if (op->address.immediate) {
                    const uint32_t shiftOffset = (op->address.imm.value & 3) * 8;
                    if (shiftOffset != 0) {
                        m_codegen.ror(dstReg32, shiftOffset);
                    }
                } else {
                    auto baseAddrReg32 = compiler.regAlloc.Get(op->address.var.var);
                    auto shiftReg32 = compiler.regAlloc.GetRCX().cvt32();
                    m_codegen.mov(shiftReg32, baseAddrReg32);
                    m_codegen.and_(shiftReg32, 3);
                    m_codegen.shl(shiftReg32, 3);
                    m_codegen.ror(dstReg32, shiftReg32.cvt8());
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
            m_codegen.mov(tcmReg64, CastUintPtr(&tcm));

            // Get temporary register for the address
            auto addrReg64 = compiler.regAlloc.GetTemporary().cvt64();

            // ITCM check
            {
                constexpr auto itcmReadSizeOfs = offsetof(arm::cp15::TCM, itcmReadSize);

                Xbyak::Label lblSkipITCM{};

                // Get address
                if (op->address.immediate) {
                    m_codegen.mov(addrReg64.cvt32(), op->address.imm.value);
                } else {
                    auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
                    m_codegen.mov(addrReg64.cvt32(), addrReg32);
                }

                // Check if address is in range
                m_codegen.cmp(addrReg64.cvt32(), dword[tcmReg64 + itcmReadSizeOfs]);
                m_codegen.jae(lblSkipITCM);

                // Compute address mirror mask
                assert(std::popcount(tcm.itcmSize) == 1); // must be a power of two
                uint32_t addrMask = tcm.itcmSize - 1;
                if (op->size == ir::MemAccessSize::Half) {
                    addrMask &= ~1;
                } else if (op->size == ir::MemAccessSize::Word) {
                    addrMask &= ~3;
                }

                // Mirror and/or align address and use it as offset into the ITCM data
                m_codegen.and_(addrReg64, addrMask);
                m_codegen.mov(tcmReg64,
                              CastUintPtr(tcm.itcm)); // Use TCM pointer register as scratch for the data pointer
                m_codegen.add(addrReg64, tcmReg64);

                // Read from ITCM
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
                compileRead(dstReg32, addrReg64);

                // Done!
                m_codegen.jmp(lblEnd);

                m_codegen.L(lblSkipITCM);
            }

            // DTCM check (data bus only)
            if (op->bus == ir::MemAccessBus::Data) {
                constexpr auto dtcmBaseOfs = offsetof(arm::cp15::TCM, dtcmBase);
                constexpr auto dtcmReadSizeOfs = offsetof(arm::cp15::TCM, dtcmReadSize);

                // Get address
                if (op->address.immediate) {
                    m_codegen.mov(addrReg64.cvt32(), op->address.imm.value);
                } else {
                    auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
                    m_codegen.mov(addrReg64.cvt32(), addrReg32);
                }

                // Adjust address to base offset
                m_codegen.sub(addrReg64.cvt32(), dword[tcmReg64 + dtcmBaseOfs]);

                // Check if address is in range
                m_codegen.cmp(addrReg64.cvt32(), dword[tcmReg64 + dtcmReadSizeOfs]);
                m_codegen.jae(lblSkipTCM);

                // Compute address mirror mask
                assert(std::popcount(tcm.dtcmSize) == 1); // must be a power of two
                uint32_t addrMask = tcm.dtcmSize - 1;
                if (op->size == ir::MemAccessSize::Half) {
                    addrMask &= ~1;
                } else if (op->size == ir::MemAccessSize::Word) {
                    addrMask &= ~3;
                }

                // Mirror and/or align address and use it as offset into the DTCM data
                m_codegen.and_(addrReg64, addrMask);
                m_codegen.mov(tcmReg64,
                              CastUintPtr(tcm.dtcm)); // Use TCM pointer register as scratch for the data pointer
                m_codegen.add(addrReg64, tcmReg64);

                // Read from DTCM
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
                compileRead(dstReg32, addrReg64);

                // Done!
                m_codegen.jmp(lblEnd);
            }
        }
    }

    // Handle slow memory access
    m_codegen.L(lblSkipTCM);

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

    m_codegen.L(lblEnd);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMemWriteOp *op) {
    // TODO: handle caches, permissions, etc.
    // TODO: fast memory LUT, including TCM blocks; replace the TCM checks below
    // TODO: virtual memory, exception handling, rewriting accessors

    Xbyak::Label lblSkipTCM{};
    Xbyak::Label lblEnd{};

    auto &cp15 = m_armState.GetSystemControlCoprocessor();
    if (cp15.IsPresent()) {
        auto &tcm = cp15.GetTCM();

        auto tcmReg64 = compiler.regAlloc.GetTemporary().cvt64();
        m_codegen.mov(tcmReg64, CastUintPtr(&tcm));

        // Get temporary register for the address
        auto addrReg64 = compiler.regAlloc.GetTemporary().cvt64();

        // ITCM check
        {
            constexpr auto itcmWriteSizeOfs = offsetof(arm::cp15::TCM, itcmWriteSize);

            Xbyak::Label lblSkipITCM{};

            // Get address
            if (op->address.immediate) {
                m_codegen.mov(addrReg64.cvt32(), op->address.imm.value);
            } else {
                auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
                m_codegen.mov(addrReg64.cvt32(), addrReg32);
            }

            // Check if address is in range
            m_codegen.cmp(addrReg64.cvt32(), dword[tcmReg64 + itcmWriteSizeOfs]);
            m_codegen.jae(lblSkipITCM);

            // Compute address mirror mask
            assert(std::popcount(tcm.itcmSize) == 1); // must be a power of two
            uint32_t addrMask = tcm.itcmSize - 1;
            if (op->size == ir::MemAccessSize::Half) {
                addrMask &= ~1;
            } else if (op->size == ir::MemAccessSize::Word) {
                addrMask &= ~3;
            }

            // Mirror and/or align address and use it as offset into the ITCM data
            m_codegen.and_(addrReg64, addrMask);
            m_codegen.mov(tcmReg64, CastUintPtr(tcm.itcm)); // Use TCM pointer register as scratch for the data pointer
            m_codegen.add(addrReg64, tcmReg64);

            // Write to ITCM
            if (op->src.immediate) {
                switch (op->size) {
                case ir::MemAccessSize::Byte: m_codegen.mov(byte[addrReg64], op->src.imm.value); break;
                case ir::MemAccessSize::Half: m_codegen.mov(word[addrReg64], op->src.imm.value); break;
                case ir::MemAccessSize::Word: m_codegen.mov(dword[addrReg64], op->src.imm.value); break;
                }
            } else {
                auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
                switch (op->size) {
                case ir::MemAccessSize::Byte: m_codegen.mov(byte[addrReg64], srcReg32.cvt8()); break;
                case ir::MemAccessSize::Half: m_codegen.mov(word[addrReg64], srcReg32.cvt16()); break;
                case ir::MemAccessSize::Word: m_codegen.mov(dword[addrReg64], srcReg32.cvt32()); break;
                }
            }

            // Done!
            m_codegen.jmp(lblEnd);

            m_codegen.L(lblSkipITCM);
        }

        // DTCM check
        {
            constexpr auto dtcmBaseOfs = offsetof(arm::cp15::TCM, dtcmBase);
            constexpr auto dtcmWriteSizeOfs = offsetof(arm::cp15::TCM, dtcmWriteSize);

            // Get address
            if (op->address.immediate) {
                m_codegen.mov(addrReg64.cvt32(), op->address.imm.value);
            } else {
                auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
                m_codegen.mov(addrReg64.cvt32(), addrReg32);
            }

            // Adjust address to base offset
            m_codegen.sub(addrReg64.cvt32(), dword[tcmReg64 + dtcmBaseOfs]);

            // Check if address is in range
            m_codegen.cmp(addrReg64.cvt32(), dword[tcmReg64 + dtcmWriteSizeOfs]);
            m_codegen.jae(lblSkipTCM);

            // Compute address mirror mask
            assert(std::popcount(tcm.dtcmSize) == 1); // must be a power of two
            uint32_t addrMask = tcm.dtcmSize - 1;
            if (op->size == ir::MemAccessSize::Half) {
                addrMask &= ~1;
            } else if (op->size == ir::MemAccessSize::Word) {
                addrMask &= ~3;
            }

            // Mirror and/or align address and use it as offset into the DTCM data
            m_codegen.and_(addrReg64, addrMask);
            m_codegen.mov(tcmReg64, CastUintPtr(tcm.dtcm)); // Use TCM pointer register as scratch for the data pointer
            m_codegen.add(addrReg64, tcmReg64);

            // Write to DTCM
            if (op->src.immediate) {
                switch (op->size) {
                case ir::MemAccessSize::Byte: m_codegen.mov(byte[addrReg64], op->src.imm.value); break;
                case ir::MemAccessSize::Half: m_codegen.mov(word[addrReg64], op->src.imm.value); break;
                case ir::MemAccessSize::Word: m_codegen.mov(dword[addrReg64], op->src.imm.value); break;
                }
            } else {
                auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
                switch (op->size) {
                case ir::MemAccessSize::Byte: m_codegen.mov(byte[addrReg64], srcReg32.cvt8()); break;
                case ir::MemAccessSize::Half: m_codegen.mov(word[addrReg64], srcReg32.cvt16()); break;
                case ir::MemAccessSize::Word: m_codegen.mov(dword[addrReg64], srcReg32.cvt32()); break;
                }
            }

            // Done!
            m_codegen.jmp(lblEnd);
        }
    }

    // Handle slow memory access
    m_codegen.L(lblSkipTCM);

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

    m_codegen.L(lblEnd);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRPreloadOp *op) {
    // TODO: implement
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLogicalShiftLeftOp *op) {
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
        m_codegen.mov(shiftReg64, 63);
        m_codegen.cmp(amountReg32, 63);
        m_codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Get destination register
        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            auto dstReg64 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            m_codegen.shlx(dstReg64, valueReg64, shiftReg64);
        } else {
            Xbyak::Reg64 dstReg{};
            if (op->dst.var.IsPresent()) {
                dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
                CopyIfDifferent(dstReg, valueReg64);
            } else {
                dstReg = compiler.regAlloc.GetTemporary().cvt64();
            }

            // Compute the shift
            m_codegen.shl(dstReg, 32); // Shift value to the top half of the 64-bit register
            m_codegen.shl(dstReg, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
            m_codegen.shr(dstReg, 32); // Shift value back down to the bottom half
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg64 = compiler.regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        m_codegen.mov(shiftReg64, 63);
        m_codegen.cmp(amountReg32, 63);
        m_codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Get destination register
        Xbyak::Reg64 dstReg64{};
        if (op->dst.var.IsPresent()) {
            dstReg64 = compiler.regAlloc.Get(op->dst.var).cvt64();
        } else {
            dstReg64 = compiler.regAlloc.GetTemporary().cvt64();
        }

        // Compute the shift
        m_codegen.mov(dstReg64, static_cast<uint64_t>(value) << 32ull);
        m_codegen.shl(dstReg64, shiftReg64.cvt8());
        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
        m_codegen.shr(dstReg64, 32); // Shift value back down to the bottom half
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value;

        if (amount < 32) {
            // Get destination register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
            } else {
                dstReg32 = compiler.regAlloc.GetTemporary();
            }

            // Compute shift and update flags
            m_codegen.shl(dstReg32, amount);
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else {
            if (amount == 32) {
                if (op->dst.var.IsPresent()) {
                    // Update carry flag before zeroing out the register
                    if (op->setCarry) {
                        m_codegen.bt(valueReg32, 0);
                        SetCFromFlags(compiler);
                    }

                    auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    m_codegen.xor_(dstReg32, dstReg32);
                } else if (op->setCarry) {
                    m_codegen.bt(valueReg32, 0);
                    SetCFromFlags(compiler);
                }
            } else {
                // Zero out destination
                if (op->dst.var.IsPresent()) {
                    auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    m_codegen.xor_(dstReg32, dstReg32);
                }
                if (op->setCarry) {
                    SetCFromValue(false);
                }
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLogicalShiftRightOp *op) {
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
        m_codegen.mov(shiftReg64, 63);
        m_codegen.cmp(amountReg32, 63);
        m_codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Compute the shift
        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            auto dstReg64 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            m_codegen.shrx(dstReg64, valueReg64, shiftReg64);
        } else if (op->dst.var.IsPresent()) {
            auto dstReg64 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            CopyIfDifferent(dstReg64, valueReg64);
            m_codegen.shr(dstReg64, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            m_codegen.dec(shiftReg64);
            m_codegen.bt(valueReg64, shiftReg64);
            SetCFromFlags(compiler);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg64 = compiler.regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        m_codegen.mov(shiftReg64, 63);
        m_codegen.cmp(amountReg32, 63);
        m_codegen.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = compiler.regAlloc.Get(op->dst.var).cvt64();
            m_codegen.mov(dstReg64, value);
            m_codegen.shr(dstReg64, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            auto valueReg64 = compiler.regAlloc.GetTemporary().cvt64();
            m_codegen.mov(valueReg64, (static_cast<uint64_t>(value) << 1ull));
            m_codegen.bt(valueReg64, shiftReg64);
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
                CopyIfDifferent(dstReg32, valueReg32);
                m_codegen.shr(dstReg32, amount);
                if (op->setCarry) {
                    SetCFromFlags(compiler);
                }
            } else if (op->setCarry) {
                m_codegen.bt(valueReg32.cvt64(), amount - 1);
                SetCFromFlags(compiler);
            }
        } else if (amount == 32) {
            if (op->dst.var.IsPresent()) {
                // Update carry flag before zeroing out the register
                if (op->setCarry) {
                    m_codegen.bt(valueReg32, 31);
                    SetCFromFlags(compiler);
                }

                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                m_codegen.xor_(dstReg32, dstReg32);
            } else if (op->setCarry) {
                m_codegen.bt(valueReg32, 31);
                SetCFromFlags(compiler);
            }
        } else {
            // Zero out destination
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                m_codegen.xor_(dstReg32, dstReg32);
            }
            if (op->setCarry) {
                SetCFromValue(false);
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRArithmeticShiftRightOp *op) {
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
        m_codegen.mov(shiftReg32, 31);
        m_codegen.cmp(amountReg32, 31);
        m_codegen.cmovbe(shiftReg32.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            m_codegen.sar(dstReg32, shiftReg32.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            m_codegen.dec(shiftReg32);
            m_codegen.bt(valueReg32, shiftReg32);
            SetCFromFlags(compiler);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg32 = compiler.regAlloc.GetRCX().cvt32();
        auto value = static_cast<int32_t>(op->value.imm.value);
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..31
        m_codegen.mov(shiftReg32, 31);
        m_codegen.cmp(amountReg32, 31);
        m_codegen.cmovbe(shiftReg32.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            m_codegen.mov(dstReg32, value);
            m_codegen.sar(dstReg32, shiftReg32.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            auto valueReg32 = compiler.regAlloc.GetTemporary();
            m_codegen.mov(valueReg32, (static_cast<uint64_t>(value) << 1ull));
            m_codegen.bt(valueReg32, shiftReg32);
            SetCFromFlags(compiler);
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amount = std::min(op->amount.imm.value, 31u);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valueReg32);
            m_codegen.sar(dstReg32, amount);
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            m_codegen.bt(valueReg32.cvt64(), amount - 1);
            SetCFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRRotateRightOp *op) {
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
        m_codegen.mov(shiftReg8, amountReg32.cvt8());

        // Put value to shift into the result register
        Xbyak::Reg32 dstReg32{};
        if (op->dst.var.IsPresent()) {
            dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valueReg32);
        } else {
            dstReg32 = compiler.regAlloc.GetTemporary();
            m_codegen.mov(dstReg32, valueReg32);
        }

        // Compute the shift
        m_codegen.ror(dstReg32, shiftReg8);
        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg8 = compiler.regAlloc.GetRCX().cvt8();
        auto value = op->value.imm.value;
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Put shift amount into CL
        m_codegen.mov(shiftReg8, amountReg32.cvt8());

        // Put value to shift into the result register
        Xbyak::Reg32 dstReg32{};
        if (op->dst.var.IsPresent()) {
            dstReg32 = compiler.regAlloc.Get(op->dst.var);
        } else if (op->setCarry) {
            dstReg32 = compiler.regAlloc.GetTemporary();
        }

        // Compute the shift
        m_codegen.mov(dstReg32, value);
        m_codegen.ror(dstReg32, shiftReg8);
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
            m_codegen.rorx(dstReg32, valueReg32, amount);
        } else {
            // Put value to shift into the result register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
            } else {
                dstReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(dstReg32, valueReg32);
            }

            // Compute the shift
            m_codegen.ror(dstReg32, amount);
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRRotateRightExtendedOp *op) {
    // ARM RRX works exactly the same as x86 RRX, including carry flag behavior.

    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 dstReg32{};

        if (op->value.immediate) {
            dstReg32 = compiler.regAlloc.Get(op->dst.var);
            m_codegen.mov(dstReg32, op->value.imm.value);
        } else {
            auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
            dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valueReg32);
        }

        m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Refresh carry flag
        m_codegen.rcr(dstReg32, 1);                   // Perform RRX

        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
    } else if (op->setCarry) {
        if (op->value.immediate) {
            SetCFromValue(bit::test<0>(op->value.imm.value));
        } else {
            auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
            m_codegen.bt(valueReg32, 0);
            SetCFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseAndOp *op) {
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
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.and_(dstReg32, imm);
            } else if (setFlags) {
                m_codegen.test(varReg32, imm);
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
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseOrOp *op) {
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
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.or_(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.or_(tmpReg32, imm);
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
                    m_codegen.or_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    m_codegen.or_(dstReg32, lhsReg32);
                } else {
                    m_codegen.mov(dstReg32, lhsReg32);
                    m_codegen.or_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.or_(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseXorOp *op) {
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
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.xor_(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.xor_(tmpReg32, imm);
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
                    m_codegen.xor_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    m_codegen.xor_(dstReg32, lhsReg32);
                } else {
                    m_codegen.mov(dstReg32, lhsReg32);
                    m_codegen.xor_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.xor_(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitClearOp *op) {
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
                CopyIfDifferent(dstReg32, rhsReg32);
                m_codegen.not_(dstReg32);

                if (lhsImm) {
                    m_codegen.and_(dstReg32, op->lhs.imm.value);
                } else {
                    auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
                    m_codegen.and_(dstReg32, lhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, rhsReg32);
                m_codegen.not_(tmpReg32);

                if (lhsImm) {
                    m_codegen.test(tmpReg32, op->lhs.imm.value);
                } else {
                    auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
                    m_codegen.test(tmpReg32, lhsReg32);
                }
            }
        } else {
            // lhs is variable, rhs is immediate
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(dstReg32, lhsReg32);
                m_codegen.and_(dstReg32, ~op->rhs.imm.value);
            } else if (setFlags) {
                m_codegen.test(lhsReg32, ~op->rhs.imm.value);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRCountLeadingZerosOp *op) {
    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 valReg32{};
        Xbyak::Reg32 dstReg32{};
        if (op->value.immediate) {
            valReg32 = compiler.regAlloc.GetTemporary();
            dstReg32 = compiler.regAlloc.Get(op->dst.var);
            m_codegen.mov(valReg32, op->value.imm.value);
        } else {
            valReg32 = compiler.regAlloc.Get(op->value.var.var);
            dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valReg32);
        }

        if (CPUID::HasLZCNT()) {
            m_codegen.lzcnt(dstReg32, valReg32);
        } else {
            // BSR unhelpfully returns the bit offset from the right, not left
            auto valIfZero32 = compiler.regAlloc.GetTemporary();
            m_codegen.mov(valIfZero32, 0xFFFFFFFF);
            m_codegen.bsr(dstReg32, valReg32);
            m_codegen.cmovz(dstReg32, valIfZero32);
            m_codegen.neg(dstReg32);
            m_codegen.add(dstReg32, 31);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddOp *op) {
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
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.add(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.add(tmpReg32, imm);
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
                    m_codegen.mov(dstReg32, lhsReg32);
                }
                m_codegen.add(dstReg32, op2Reg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.add(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags();
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddCarryOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            m_codegen.mov(dstReg32, op->lhs.imm.value);

            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.adc(dstReg32, op->rhs.imm.value);
            if (setFlags) {
                SetNZCVFromFlags();
            }
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                m_codegen.adc(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                m_codegen.adc(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    m_codegen.mov(dstReg32, lhsReg32);
                }
                m_codegen.adc(dstReg32, op2Reg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
                m_codegen.adc(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags();
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSubtractOp *op) {
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
                CopyIfDifferent(dstReg32, lhsReg32);

                if (rhsImm) {
                    m_codegen.sub(dstReg32, op->rhs.imm.value);
                } else {
                    auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
                    m_codegen.sub(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                if (rhsImm) {
                    m_codegen.cmp(lhsReg32, op->rhs.imm.value);
                } else {
                    auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
                    m_codegen.cmp(lhsReg32, rhsReg32);
                }
            }
        } else {
            // lhs is immediate, rhs is variable
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
                m_codegen.mov(dstReg32, op->lhs.imm.value);
                m_codegen.sub(dstReg32, rhsReg32);
            } else if (setFlags) {
                auto lhsReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(lhsReg32, op->lhs.imm.value);
                m_codegen.cmp(lhsReg32, rhsReg32);
            }
        }

        if (setFlags) {
            m_codegen.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
            SetNZCVFromFlags();
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSubtractCarryOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    // Note: x86 and ARM have inverted borrow bits

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            m_codegen.mov(dstReg32, op->lhs.imm.value);

            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.cmc();                              // Complement it
            m_codegen.sbb(dstReg32, op->rhs.imm.value);
            if (setFlags) {
                m_codegen.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
                SetNZCVFromFlags();
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
                CopyIfDifferent(dstReg32, lhsReg32);
            } else {
                dstReg32 = compiler.regAlloc.GetTemporary();
            }

            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.cmc();                              // Complement it
            if (rhsImm) {
                m_codegen.sbb(dstReg32, op->rhs.imm.value);
            } else {
                auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
                m_codegen.sbb(dstReg32, rhsReg32);
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
            m_codegen.mov(dstReg32, op->lhs.imm.value);
            m_codegen.bt(abi::kHostFlagsReg, x64flgCPos); // Load carry flag
            m_codegen.cmc();                              // Complement it
            m_codegen.sbb(dstReg32, rhsReg32);
        }

        if (setFlags) {
            m_codegen.cmc(); // Complement carry output
            SetNZCVFromFlags();
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMoveOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg32, op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valReg32);
        }

        if (setFlags) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            SetNZFromReg(compiler, dstReg32);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            SetNZFromReg(compiler, valReg32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMoveNegatedOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg32, ~op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);

            CopyIfDifferent(dstReg32, valReg32);
            m_codegen.not_(dstReg32);
        }

        if (setFlags) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            SetNZFromReg(compiler, dstReg32);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(~op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            auto tmpReg32 = compiler.regAlloc.GetTemporary();
            m_codegen.mov(tmpReg32, valReg32);
            m_codegen.not_(tmpReg32);
            SetNZFromReg(compiler, tmpReg32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSaturatingAddOp *op) {
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
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.add(dstReg32, imm);
                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(maxValReg32, std::numeric_limits<int32_t>::max());
                m_codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.add(tmpReg32, imm);
                SetVFromFlags();
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
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(maxValReg32, std::numeric_limits<int32_t>::max());
                m_codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.add(tmpReg32, rhsReg32);
                SetVFromFlags();
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSaturatingSubtractOp *op) {
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
                CopyIfDifferent(dstReg32, varReg32);
                m_codegen.sub(dstReg32, imm);
                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(maxValReg32, std::numeric_limits<int32_t>::min());
                m_codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, varReg32);
                m_codegen.sub(tmpReg32, imm);
                SetVFromFlags();
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
                    m_codegen.sub(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    m_codegen.sub(dstReg32, lhsReg32);
                } else {
                    m_codegen.mov(dstReg32, lhsReg32);
                    m_codegen.sub(dstReg32, rhsReg32);
                }

                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(maxValReg32, std::numeric_limits<int32_t>::min());
                m_codegen.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                m_codegen.mov(tmpReg32, lhsReg32);
                m_codegen.sub(tmpReg32, rhsReg32);
                SetVFromFlags();
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMultiplyOp *op) {
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
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
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
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

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
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

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
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMultiplyLongOp *op) {
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
                    m_codegen.movsxd(dstReg64, varReg32);
                } else if (dstReg64.cvt32() != varReg32) {
                    m_codegen.mov(dstReg64.cvt32(), varReg32);
                }

                // Multiply and shift down if needed
                // If dstLo is present, the result is already in place
                if (op->signedMul) {
                    m_codegen.imul(dstReg64, dstReg64, static_cast<int32_t>(imm));
                } else {
                    m_codegen.imul(dstReg64, dstReg64, imm);
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
                    auto dstHiReg64 = compiler.regAlloc.Get(op->dstHi.var).cvt64();
                    if (CPUID::HasBMI2()) {
                        m_codegen.rorx(dstHiReg64, dstReg64, 32);
                    } else {
                        m_codegen.mov(dstHiReg64, dstReg64);
                        m_codegen.shr(dstHiReg64, 32);
                    }
                }

                if (setFlags) {
                    m_codegen.test(dstReg64, dstReg64); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg64 = compiler.regAlloc.GetTemporary().cvt64();
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
                        m_codegen.movsxd(dstReg64, lhsReg32);
                    } else {
                        m_codegen.mov(dstReg64.cvt32(), lhsReg32);
                    }
                } else if (op->signedMul) {
                    m_codegen.movsxd(dstReg64, dstReg64.cvt32());
                }

                if (op->signedMul) {
                    m_codegen.movsxd(op2Reg32.cvt64(), op2Reg32);
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
                    auto dstHiReg64 = compiler.regAlloc.Get(op->dstHi.var).cvt64();
                    if (CPUID::HasBMI2()) {
                        m_codegen.rorx(dstHiReg64, dstReg64, 32);
                    } else {
                        m_codegen.mov(dstHiReg64, dstReg64);
                        m_codegen.shr(dstHiReg64, 32);
                    }
                }

                if (setFlags) {
                    m_codegen.test(dstReg64, dstReg64); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg64 = compiler.regAlloc.GetTemporary().cvt64();
                auto op2Reg64 = compiler.regAlloc.GetTemporary().cvt64();

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
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddLongOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();

    // Contains the value 32 to be used in shifts
    Xbyak::Reg64 shiftBy32Reg64{};
    if (CPUID::HasBMI2()) {
        shiftBy32Reg64 = compiler.regAlloc.GetTemporary().cvt64();
        m_codegen.mov(shiftBy32Reg64, 32);
    }

    // Compose two input variables (lo and hi) into a single 64-bit register
    auto compose64 = [&](const ir::VarOrImmArg &lo, const ir::VarOrImmArg &hi) {
        auto outReg64 = compiler.regAlloc.GetTemporary().cvt64();
        if (lo.immediate && hi.immediate) {
            // Both are immediates
            const uint64_t value = static_cast<uint64_t>(lo.imm.value) | (static_cast<uint64_t>(hi.imm.value) << 32ull);
            m_codegen.mov(outReg64, value);
        } else if (!lo.immediate && !hi.immediate) {
            // Both are variables
            auto loReg64 = compiler.regAlloc.Get(lo.var.var).cvt64();
            auto hiReg64 = compiler.regAlloc.Get(hi.var.var).cvt64();

            if (CPUID::HasBMI2()) {
                m_codegen.shlx(outReg64, hiReg64, shiftBy32Reg64);
            } else {
                m_codegen.mov(outReg64, hiReg64);
                m_codegen.shl(outReg64, 32);
            }
            m_codegen.or_(outReg64, loReg64);
        } else if (lo.immediate) {
            // lo is immediate, hi is variable
            auto hiReg64 = compiler.regAlloc.Get(hi.var.var).cvt64();

            if (outReg64 != hiReg64 && CPUID::HasBMI2()) {
                m_codegen.shlx(outReg64, hiReg64, shiftBy32Reg64);
            } else {
                CopyIfDifferent(outReg64, hiReg64);
                m_codegen.shl(outReg64, 32);
            }
            m_codegen.or_(outReg64, lo.imm.value);
        } else {
            // lo is variable, hi is immediate
            auto loReg64 = compiler.regAlloc.Get(lo.var.var).cvt64();

            if (outReg64 != loReg64 && CPUID::HasBMI2()) {
                m_codegen.shlx(outReg64, loReg64, shiftBy32Reg64);
            } else {
                CopyIfDifferent(outReg64, loReg64);
                m_codegen.shl(outReg64, 32);
            }
            m_codegen.or_(outReg64, hi.imm.value);
            m_codegen.ror(outReg64, 32);
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
        m_codegen.mov(dstLoReg64, lhsReg64);
    }
    m_codegen.add(dstLoReg64, rhsReg64);

    // Update flags if requested
    if (setFlags) {
        SetNZFromFlags(compiler);
    }

    // Put top half of the result into dstHi if it is present
    if (op->dstHi.var.IsPresent()) {
        auto dstHiReg64 = compiler.regAlloc.Get(op->dstHi.var).cvt64();
        if (CPUID::HasBMI2()) {
            m_codegen.shrx(dstHiReg64, dstLoReg64, shiftBy32Reg64);
        } else {
            m_codegen.mov(dstHiReg64, dstLoReg64);
            m_codegen.shr(dstHiReg64, 32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRStoreFlagsOp *op) {
    if (op->flags != arm::Flags::None) {
        const auto mask = static_cast<uint32_t>(op->flags) >> ARMflgNZCVShift;
        if (op->values.immediate) {
            const auto value = op->values.imm.value >> ARMflgNZCVShift;
            const auto ones = ((value & mask) * ARMTox64FlagsMult) & x64FlagsMask;
            const auto zeros = ((~value & mask) * ARMTox64FlagsMult) & x64FlagsMask;
            if (ones != 0) {
                m_codegen.or_(abi::kHostFlagsReg, ones);
            }
            if (zeros != 0) {
                m_codegen.and_(abi::kHostFlagsReg, ~zeros);
            }
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->values.var.var);
            auto maskReg32 = compiler.regAlloc.GetTemporary();
            m_codegen.shr(valReg32, ARMflgNZCVShift);
            m_codegen.imul(valReg32, valReg32, ARMTox64FlagsMult);
            m_codegen.and_(valReg32, x64FlagsMask);
            m_codegen.mov(maskReg32, (~mask * ARMTox64FlagsMult) & x64FlagsMask);
            m_codegen.and_(abi::kHostFlagsReg, maskReg32);
            m_codegen.or_(abi::kHostFlagsReg, valReg32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadFlagsOp *op) {
    // Get value from srcCPSR and copy to dstCPSR, or reuse register from srcCPSR if possible
    if (op->srcCPSR.immediate) {
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);
        m_codegen.mov(dstReg32, op->srcCPSR.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(dstReg32, srcReg32);
    }

    // Apply flags to dstReg32
    if (BitmaskEnum(op->flags).Any()) {
        const uint32_t cpsrMask = static_cast<uint32_t>(op->flags);
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);

        // Extract the host flags we need from EAX into flags
        auto flags = compiler.regAlloc.GetTemporary();
        if (CPUID::HasFastPDEPAndPEXT()) {
            m_codegen.mov(flags, x64FlagsMask);
            m_codegen.pext(flags, abi::kHostFlagsReg, flags);
            m_codegen.shl(flags, 28);
        } else {
            m_codegen.imul(flags, abi::kHostFlagsReg, x64ToARMFlagsMult);
            // m_codegen.and_(flags, ARMFlagsMask);
        }
        m_codegen.and_(flags, cpsrMask);     // Keep only the affected bits
        m_codegen.and_(dstReg32, ~cpsrMask); // Clear affected bits from dst value
        m_codegen.or_(dstReg32, flags);      // Store new bits into dst value
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadStickyOverflowOp *op) {
    if (op->setQ) {
        auto srcReg32 = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);

        // Apply overflow flag in the Q position
        m_codegen.mov(dstReg32, abi::kHostFlagsReg.cvt8()); // Copy overflow flag into destination register
        m_codegen.shl(dstReg32, ARMflgQPos);                // Move Q into position
        m_codegen.or_(dstReg32, srcReg32);                  // OR with srcCPSR
    } else if (op->srcCPSR.immediate) {
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);
        m_codegen.mov(dstReg32, op->srcCPSR.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(dstReg32, srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBranchOp *op) {
    const auto pcFieldOffset = m_armState.GPROffset(arm::GPR::PC, compiler.mode);
    const auto pcOffset = 2 * (compiler.thumb ? sizeof(uint16_t) : sizeof(uint32_t));
    const auto addrMask = (compiler.thumb ? ~1 : ~3);

    if (op->address.immediate) {
        m_codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], (op->address.imm.value & addrMask) + pcOffset);
    } else {
        auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
        auto tmpReg32 = compiler.regAlloc.GetTemporary();
        m_codegen.lea(tmpReg32, dword[addrReg32 + pcOffset]);
        m_codegen.and_(tmpReg32, addrMask);
        m_codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], tmpReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBranchExchangeOp *op) {
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
            m_codegen.mov(pcReg32.cvt64(), CastUintPtr(&cp15ctl));
            m_codegen.test(dword[pcReg32.cvt64() + ctlValueOfs], (1 << 15)); // L4 bit
            m_codegen.je(lblExchange);

            // Perform branch without exchange
            const auto pcOffset = 2 * (compiler.thumb ? sizeof(uint16_t) : sizeof(uint32_t));
            const auto addrMask = (compiler.thumb ? ~1 : ~3);
            if (op->address.immediate) {
                m_codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], (op->address.imm.value & addrMask) + pcOffset);
            } else {
                auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);
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
            m_codegen.or_(dword[abi::kARMStateReg + cpsrFieldOffset], (1 << 5)); // T bit
            m_codegen.mov(pcReg32, (op->address.imm.value & ~1) + 2 * sizeof(uint16_t));
        } else {
            // ARM branch
            m_codegen.and_(dword[abi::kARMStateReg + cpsrFieldOffset], ~(1 << 5)); // T bit
            m_codegen.mov(pcReg32, (op->address.imm.value & ~3) + 2 * sizeof(uint32_t));
        }
    } else {
        Xbyak::Label lblBranchARM;
        Xbyak::Label lblSetPC;

        auto addrReg32 = compiler.regAlloc.Get(op->address.var.var);

        // Determine if this is a Thumb or ARM branch based on bit 0 of the given address
        m_codegen.test(addrReg32, 1);
        m_codegen.je(lblBranchARM);

        // Thumb branch
        m_codegen.or_(dword[abi::kARMStateReg + cpsrFieldOffset], (1 << 5));
        m_codegen.lea(pcReg32, dword[addrReg32 + 2 * sizeof(uint16_t) - 1]);
        // The address always has bit 0 set, so (addr & ~1) == (addr - 1)
        // Therefore, (addr & ~1) + 4 == (addr - 1) + 4 == (addr + 3)
        // m_codegen.lea(pcReg32, dword[addrReg32 + 2 * sizeof(uint16_t)]);
        // m_codegen.and_(pcReg32, ~1);
        m_codegen.jmp(lblSetPC);

        // ARM branch
        m_codegen.L(lblBranchARM);
        m_codegen.and_(dword[abi::kARMStateReg + cpsrFieldOffset], ~(1 << 5));
        m_codegen.lea(pcReg32, dword[addrReg32 + 2 * sizeof(uint32_t)]);
        m_codegen.and_(pcReg32, ~3);

        m_codegen.L(lblSetPC);
    }

    // Set PC to branch target
    m_codegen.mov(dword[abi::kARMStateReg + pcFieldOffset], pcReg32);

    m_codegen.L(lblEnd);
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
    if (op->dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
        m_codegen.mov(dstReg32, op->value);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRCopyVarOp *op) {
    // This instruction should be optimized away, but here's an implementation anyway
    if (!op->var.var.IsPresent() || !op->dst.var.IsPresent()) {
        return;
    }
    auto varReg32 = compiler.regAlloc.Get(op->var.var);
    auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->var.var);
    CopyIfDifferent(dstReg32, varReg32);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetBaseVectorAddressOp *op) {
    if (op->dst.var.IsPresent()) {
        auto &cp15 = m_armState.GetSystemControlCoprocessor();
        if (cp15.IsPresent()) {
            // Load base vector address from CP15
            auto &cp15ctl = cp15.GetControlRegister();
            const auto baseVectorAddressOfs = offsetof(arm::cp15::ControlRegister, baseVectorAddress);

            auto ctlPtrReg64 = compiler.regAlloc.GetTemporary().cvt64();
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            m_codegen.mov(ctlPtrReg64, CastUintPtr(&cp15ctl));
            m_codegen.mov(dstReg32, dword[ctlPtrReg64 + baseVectorAddressOfs]);
        } else {
            // Default to 00000000 if CP15 is absent
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            m_codegen.xor_(dstReg32, dstReg32);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void x64Host::SetCFromValue(bool carry) {
    if (carry) {
        m_codegen.or_(abi::kHostFlagsReg, x64flgC);
    } else {
        m_codegen.and_(abi::kHostFlagsReg, ~x64flgC);
    }
}

void x64Host::SetCFromFlags(Compiler &compiler) {
    auto tmp32 = compiler.regAlloc.GetTemporary();
    m_codegen.setc(tmp32.cvt8());                 // Put new C into a temporary register
    m_codegen.movzx(tmp32, tmp32.cvt8());         // Zero-extend to 32 bits
    m_codegen.shl(tmp32, x64flgCPos);             // Move it to the correct position
    m_codegen.and_(abi::kHostFlagsReg, ~x64flgC); // Clear existing C flag from EAX
    m_codegen.or_(abi::kHostFlagsReg, tmp32);     // Write new C flag into EAX
}

void x64Host::SetVFromValue(bool overflow) {
    if (overflow) {
        m_codegen.mov(abi::kHostFlagsReg.cvt8(), 1);
    } else {
        m_codegen.xor_(abi::kHostFlagsReg.cvt8(), abi::kHostFlagsReg.cvt8());
    }
}

void x64Host::SetVFromFlags() {
    m_codegen.seto(abi::kHostFlagsReg.cvt8());
}

void x64Host::SetNZFromValue(uint32_t value) {
    const bool n = (value >> 31u);
    const bool z = (value == 0);
    const uint32_t ones = (n * x64flgN) | (z * x64flgZ);
    const uint32_t zeros = (!n * x64flgN) | (!z * x64flgZ);
    if (ones != 0) {
        m_codegen.or_(abi::kHostFlagsReg, ones);
    }
    if (zeros != 0) {
        m_codegen.and_(abi::kHostFlagsReg, ~zeros);
    }
}

void x64Host::SetNZFromValue(uint64_t value) {
    const bool n = (value >> 63ull);
    const bool z = (value == 0);
    const uint32_t ones = (n * x64flgN) | (z * x64flgZ);
    const uint32_t zeros = (!n * x64flgN) | (!z * x64flgZ);
    if (ones != 0) {
        m_codegen.or_(abi::kHostFlagsReg, ones);
    }
    if (zeros != 0) {
        m_codegen.and_(abi::kHostFlagsReg, ~zeros);
    }
}

void x64Host::SetNZFromReg(Compiler &compiler, Xbyak::Reg32 value) {
    auto tmp32 = compiler.regAlloc.GetTemporary();
    m_codegen.test(value, value);             // Updates NZ, clears CV; V won't be changed here
    m_codegen.mov(tmp32, abi::kHostFlagsReg); // Copy current flags to preserve C later
    m_codegen.lahf();                         // Load NZC; C is 0
    m_codegen.and_(tmp32, x64flgC);           // Keep previous C only
    m_codegen.or_(abi::kHostFlagsReg, tmp32); // Put previous C into AH; NZ is now updated and C is preserved
}

void x64Host::SetNZFromFlags(Compiler &compiler) {
    auto tmp32 = compiler.regAlloc.GetTemporary();
    m_codegen.clc();                          // Clear C to make way for the previous C
    m_codegen.mov(tmp32, abi::kHostFlagsReg); // Copy current flags to preserve C later
    m_codegen.lahf();                         // Load NZC; C is 0
    m_codegen.and_(tmp32, x64flgC);           // Keep previous C only
    m_codegen.or_(abi::kHostFlagsReg, tmp32); // Put previous C into AH; NZ is now updated and C is preserved
}

void x64Host::SetNZCVFromValue(uint32_t value, bool carry, bool overflow) {
    const bool n = (value >> 31u);
    const bool z = (value == 0);
    const uint32_t ones = (n * x64flgN) | (z * x64flgZ) | (carry * x64flgC);
    const uint32_t zeros = (!n * x64flgN) | (!z * x64flgZ) | (!carry * x64flgC);
    if (ones != 0) {
        m_codegen.or_(abi::kHostFlagsReg, ones);
    }
    if (zeros != 0) {
        m_codegen.and_(abi::kHostFlagsReg, ~zeros);
    }
    m_codegen.mov(abi::kHostFlagsReg.cvt8(), static_cast<uint8_t>(overflow));
}

void x64Host::SetNZCVFromFlags() {
    m_codegen.lahf();
    m_codegen.seto(abi::kHostFlagsReg.cvt8());
}

void x64Host::MOVImmediate(Xbyak::Reg32 reg, uint32_t value) {
    if (value == 0) {
        m_codegen.xor_(reg, reg);
    } else {
        m_codegen.mov(reg, value);
    }
}

void x64Host::CopyIfDifferent(Xbyak::Reg32 dst, Xbyak::Reg32 src) {
    if (dst != src) {
        m_codegen.mov(dst, src);
    }
}

void x64Host::CopyIfDifferent(Xbyak::Reg64 dst, Xbyak::Reg64 src) {
    if (dst != src) {
        m_codegen.mov(dst, src);
    }
}

void x64Host::AssignImmResultWithNZ(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setFlags) {
        SetNZFromValue(result);
    }
}

void x64Host::AssignImmResultWithNZCV(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool carry,
                                      bool overflow, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setFlags) {
        SetNZCVFromValue(result, carry, overflow);
    }
}

void x64Host::AssignImmResultWithCarry(Compiler &compiler, const ir::VariableArg &dst, uint32_t result,
                                       std::optional<bool> carry, bool setCarry) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setCarry && carry) {
        SetCFromValue(*carry);
    }
}

void x64Host::AssignImmResultWithOverflow(Compiler &compiler, const ir::VariableArg &dst, uint32_t result,
                                          bool overflow, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setFlags) {
        SetVFromValue(overflow);
    }
}

void x64Host::AssignLongImmResultWithNZ(Compiler &compiler, const ir::VariableArg &dstLo, const ir::VariableArg &dstHi,
                                        uint64_t result, bool setFlags) {
    if (dstLo.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dstLo.var);
        MOVImmediate(dstReg32, result);
    }
    if (dstHi.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dstHi.var);
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
void x64Host::CompileInvokeHostFunctionImpl(Compiler &compiler, Xbyak::Reg dstReg, ReturnType (*fn)(FnArgs...),
                                            Args &&...args) {
    static_assert(is_raw_integral_v<ReturnType> || std::is_void_v<ReturnType> || std::is_pointer_v<ReturnType> ||
                      std::is_reference_v<ReturnType>,
                  "ReturnType must be an integral type, void, pointer or reference");

    static_assert(((is_raw_integral_v<FnArgs> || is_raw_base_of_v<Xbyak::Operand, FnArgs> ||
                    std::is_pointer_v<FnArgs> || std::is_reference_v<FnArgs>)&&...),
                  "All FnArgs must be integral types, Xbyak operands, pointers or references");

    // Save the return value register
    m_codegen.push(abi::kIntReturnValueReg);

    // Save all used volatile registers
    std::vector<Xbyak::Reg64> savedRegs;
    for (auto reg : abi::kVolatileRegs) {
        // The return register is handled on its own
        if (reg == abi::kIntReturnValueReg) {
            continue;
        }

        // Only push allocated registers
        if (compiler.regAlloc.IsRegisterAllocated(reg)) {
            m_codegen.push(reg);
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
                    m_codegen.mov(argReg64, arg.cvt64());
                }
            } else if constexpr (is_raw_integral_v<TArg>) {
                m_codegen.mov(argReg64, arg);
            } else if constexpr (std::is_pointer_v<TArg>) {
                m_codegen.mov(argReg64, CastUintPtr(arg));
            } else if constexpr (std::is_reference_v<TArg>) {
                m_codegen.mov(argReg64, CastUintPtr(&arg));
            } else {
                m_codegen.mov(argReg64, arg);
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
        m_codegen.sub(rsp, stackAlignmentOffset);
    }

    // Call host function using the return value register as a pointer
    m_codegen.mov(abi::kIntReturnValueReg, CastUintPtr(fn));
    m_codegen.call(abi::kIntReturnValueReg);

    // Undo stack alignment
    if (stackAlignmentOffset != 0) {
        m_codegen.add(rsp, stackAlignmentOffset);
    }

    // Pop all saved registers
    for (auto it = savedRegs.rbegin(); it != savedRegs.rend(); it++) {
        m_codegen.pop(*it);
    }

    // Copy result to destination register if present
    if constexpr (!std::is_void_v<ReturnType>) {
        if (!dstReg.isNone()) {
            m_codegen.mov(dstReg, abi::kIntReturnValueReg.changeBit(dstReg.getBit()));
        }
    }

    // Pop the return value register
    m_codegen.pop(abi::kIntReturnValueReg);
}

} // namespace armajitto::x86_64
