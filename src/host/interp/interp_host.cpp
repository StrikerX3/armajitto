#include "interp_host.hpp"

#include "armajitto/guest/arm/exceptions.hpp"
#include "guest/arm/arithmetic.hpp"

#include "ir/ops/ir_ops_visitor.hpp"

#include "util/bit_ops.hpp"
#include "util/unreachable.hpp"

#include <bit>

namespace armajitto::interp {

InterpreterHost::InterpreterHost(Context &context, Options::Compiler &options, std::pmr::memory_resource &alloc)
    : Host(context, options)
    , m_alloc(alloc)
    , m_armState(context.GetARMState())
    , m_memMap(context.GetSystem().GetMemoryMap()) {

    SetInvalidateCodeCacheCallback(
        [](uint32_t start, uint32_t end, void *ctx) {
            auto &host = *reinterpret_cast<InterpreterHost *>(ctx);
            host.InvalidateCodeCacheRange(start, end);
        },
        this);
}

InterpreterHost::~InterpreterHost() {}

HostCode InterpreterHost::Compile(ir::BasicBlock &block) {
    const uint64_t key = block.Location().ToUint64();

    auto &compiledBlock = m_blockCache[key];
    compiledBlock.cond = block.Condition();
    compiledBlock.passCycles = block.PassCycles();
    compiledBlock.failCycles = block.FailCycles();
    compiledBlock.loc = block.Location();
    compiledBlock.instrCount = block.InstructionCount();
    compiledBlock.instrs.clear();

    auto *op = block.Head();
    while (op != nullptr) {
        ir::VisitIROp(op, [&, this](const auto *op) -> void { compiledBlock.instrs.push_back(CompileOp(op)); });
        op = op->Next();
    }

    return HostCode(key);
}

void InterpreterHost::Clear() {
    m_blockCache.clear();
    std::fill(m_vars.begin(), m_vars.end(), 0);
    m_flags = arm::Flags::None;
    m_flagQ = false;
}

void InterpreterHost::Invalidate(LocationRef loc) {
    m_blockCache.erase(loc.ToUint64());
}

void InterpreterHost::InvalidateCodeCache() {
    m_blockCache.clear();
}

void InterpreterHost::InvalidateCodeCacheRange(uint32_t start, uint32_t end) {
    if (start == 0 && end == 0xFFFFFFFF) {
        InvalidateCodeCache();
        return;
    }

    for (uint64_t cpsr = 0; cpsr < 63; cpsr++) {
        const uint64_t upper = cpsr << 32ull;
        for (uint64_t addr = start; addr <= end; addr += 2) {
            const uint64_t key = addr | upper;
            m_blockCache.erase(key);
        }
    }
}

void InterpreterHost::ReportMemoryWrite(uint32_t start, uint32_t end) {
    InvalidateCodeCacheRange(start, end);
}

void InterpreterHost::SetVar(ir::Variable var, uint32_t value) {
    if (!var.IsPresent()) {
        return;
    }
    const auto index = var.Index();
    if (index >= m_vars.size()) {
        m_vars.resize(index + 1);
    }
    m_vars[index] = value;
}

uint32_t InterpreterHost::GetVar(ir::Variable var) {
    if (!var.IsPresent()) {
        return 0;
    }
    const auto index = var.Index();
    if (index >= m_vars.size()) {
        return 0;
    }
    return m_vars[index];
}

uint32_t InterpreterHost::Get(ir::VarOrImmArg arg) {
    if (arg.immediate) {
        return Get(arg.imm);
    } else {
        return Get(arg.var);
    }
}

uint32_t InterpreterHost::Get(ir::VariableArg arg) {
    return GetVar(arg.var);
}

uint32_t InterpreterHost::Get(ir::ImmediateArg arg) {
    return arg.value;
}

int64_t InterpreterHost::Execute(const CompiledBlock &block) {
    auto evalCondition = [&](arm::Condition cond) {
        auto bmFlags = BitmaskEnum(m_flags);
        const bool n = bmFlags.AnyOf(arm::Flags::N);
        const bool z = bmFlags.AnyOf(arm::Flags::Z);
        const bool c = bmFlags.AnyOf(arm::Flags::C);
        const bool v = bmFlags.AnyOf(arm::Flags::V);

        switch (cond) {
        case arm::Condition::EQ: return z;
        case arm::Condition::NE: return !z;
        case arm::Condition::CS: return c;
        case arm::Condition::CC: return !c;
        case arm::Condition::MI: return n;
        case arm::Condition::PL: return !n;
        case arm::Condition::VS: return v;
        case arm::Condition::VC: return !v;
        case arm::Condition::HI: return c && !z;
        case arm::Condition::LS: return !c || z;
        case arm::Condition::GE: return n == v;
        case arm::Condition::LT: return n != v;
        case arm::Condition::GT: return !z && n == v;
        case arm::Condition::LE: return z || n != v;
        case arm::Condition::AL: return true;
        case arm::Condition::NV: return false;
        default: util::unreachable();
        }
    };

    if (evalCondition(block.cond)) {
        for (auto &instr : block.instrs) {
            (this->*instr.fn)(instr.op);
        }
        return block.passCycles;
    } else {
        const uint32_t instrSize = block.loc.IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t);
        m_armState.GPR(arm::GPR::PC) = block.loc.PC() + block.instrCount * instrSize;
        return block.failCycles;
    }
}

auto InterpreterHost::CompileOp(const ir::IRGetRegisterOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleGetRegister, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRSetRegisterOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleSetRegister, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRGetCPSROp *op) -> InterpInstr {
    return {&InterpreterHost::HandleGetCPSR, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRSetCPSROp *op) -> InterpInstr {
    return {&InterpreterHost::HandleSetCPSR, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRGetSPSROp *op) -> InterpInstr {
    return {&InterpreterHost::HandleGetSPSR, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRSetSPSROp *op) -> InterpInstr {
    return {&InterpreterHost::HandleSetSPSR, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRMemReadOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleMemRead, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRMemWriteOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleMemWrite, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRPreloadOp *op) -> InterpInstr {
    return {&InterpreterHost::HandlePreload, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRLogicalShiftLeftOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleLogicalShiftLeft, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRLogicalShiftRightOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleLogicalShiftRight, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRArithmeticShiftRightOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleArithmeticShiftRight, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRRotateRightOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleRotateRight, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRRotateRightExtendedOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleRotateRightExtended, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRBitwiseAndOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleBitwiseAnd, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRBitwiseOrOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleBitwiseOr, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRBitwiseXorOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleBitwiseXor, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRBitClearOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleBitClear, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRCountLeadingZerosOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleCountLeadingZeros, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRAddOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleAdd, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRAddCarryOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleAddCarry, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRSubtractOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleSubtract, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRSubtractCarryOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleSubtractCarry, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRMoveOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleMove, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRMoveNegatedOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleMoveNegated, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRSaturatingAddOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleSaturatingAdd, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRSaturatingSubtractOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleSaturatingSubtract, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRMultiplyOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleMultiply, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRMultiplyLongOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleMultiplyLong, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRAddLongOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleAddLong, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRStoreFlagsOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleStoreFlags, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRLoadFlagsOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleLoadFlags, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRLoadStickyOverflowOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleLoadStickyOverflow, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRBranchOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleBranch, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRBranchExchangeOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleBranchExchange, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRLoadCopRegisterOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleLoadCopRegister, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRStoreCopRegisterOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleStoreCopRegister, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRConstantOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleConstant, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRCopyVarOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleCopyVar, {*op}};
}

auto InterpreterHost::CompileOp(const ir::IRGetBaseVectorAddressOp *op) -> InterpInstr {
    return {&InterpreterHost::HandleGetBaseVectorAddress, {*op}};
}

// -------------------------------------------------------------------------------------------------------------------

static inline void UpdateFlags(arm::Flags &dst, arm::Flags mask, bool value) {
    if (value) {
        dst |= mask;
    } else {
        dst &= ~mask;
    }
}

static inline void UpdateNZ(arm::Flags &dst, arm::Flags mask, uint32_t result) {
    const auto bmFlags = BitmaskEnum(mask);
    if (bmFlags.AnyOf(arm::Flags::N)) {
        UpdateFlags(dst, arm::Flags::N, result >> 31u);
    }
    if (bmFlags.AnyOf(arm::Flags::Z)) {
        UpdateFlags(dst, arm::Flags::Z, result == 0);
    }
}

static inline void UpdateNZLong(arm::Flags &dst, arm::Flags mask, uint64_t result) {
    const auto bmFlags = BitmaskEnum(mask);
    if (bmFlags.AnyOf(arm::Flags::N)) {
        UpdateFlags(dst, arm::Flags::N, result >> 63ull);
    }
    if (bmFlags.AnyOf(arm::Flags::Z)) {
        UpdateFlags(dst, arm::Flags::Z, result == 0);
    }
}

static inline void UpdateNZCV(arm::Flags &dst, arm::Flags mask, uint32_t result, bool carry, bool overflow) {
    UpdateNZ(dst, mask, result);
    const auto bmFlags = BitmaskEnum(mask);
    if (bmFlags.AnyOf(arm::Flags::C)) {
        UpdateFlags(dst, arm::Flags::C, carry);
    }
    if (bmFlags.AnyOf(arm::Flags::V)) {
        UpdateFlags(dst, arm::Flags::V, overflow);
    }
}

void InterpreterHost::HandleGetRegister(const Op &varOp) {
    auto &op = std::get<ir::IRGetRegisterOp>(varOp);
    SetVar(op.dst.var, m_armState.GPR(op.src.gpr, op.src.Mode()));
}

void InterpreterHost::HandleSetRegister(const Op &varOp) {
    auto &op = std::get<ir::IRSetRegisterOp>(varOp);
    m_armState.GPR(op.dst.gpr, op.dst.Mode()) = Get(op.src);
}

void InterpreterHost::HandleGetCPSR(const Op &varOp) {
    auto &op = std::get<ir::IRGetCPSROp>(varOp);
    SetVar(op.dst.var, m_armState.CPSR().u32);
}

void InterpreterHost::HandleSetCPSR(const Op &varOp) {
    auto &op = std::get<ir::IRSetCPSROp>(varOp);
    m_armState.CPSR().u32 = Get(op.src);
    m_armState.SetMode(m_armState.CPSR().mode);
}
void InterpreterHost::HandleGetSPSR(const Op &varOp) {
    auto &op = std::get<ir::IRGetSPSROp>(varOp);
    SetVar(op.dst.var, m_armState.SPSR(op.mode).u32);
}

void InterpreterHost::HandleSetSPSR(const Op &varOp) {
    auto &op = std::get<ir::IRSetSPSROp>(varOp);
    m_armState.SPSR(op.mode).u32 = Get(op.src);
}

void InterpreterHost::HandleMemRead(const Op &varOp) {
    auto &op = std::get<ir::IRMemReadOp>(varOp);
    auto &sys = m_context.GetSystem();
    const auto addr = Get(op.address);

    auto &mem = op.bus == ir::MemAccessBus::Code ? m_memMap.codeRead : m_memMap.dataRead;

    auto readByte = [&](uint32_t addr) {
        auto *ptr = mem.GetPointer<uint8_t>(addr);
        if (ptr != nullptr) {
            return *ptr;
        } else {
            return sys.MemReadByte(addr);
        }
    };

    auto readHalf = [&](uint32_t addr) {
        addr &= ~1;
        auto *ptr = mem.GetPointer<uint16_t>(addr);
        if (ptr != nullptr) {
            return *ptr;
        } else {
            return sys.MemReadHalf(addr);
        }
    };

    auto readWord = [&](uint32_t addr) {
        addr &= ~3;
        auto *ptr = mem.GetPointer<uint32_t>(addr);
        if (ptr != nullptr) {
            return *ptr;
        } else {
            return sys.MemReadWord(addr);
        }
    };

    uint32_t value = 0;
    switch (op.size) {
    case ir::MemAccessSize::Byte:
        value = readByte(addr);
        if (op.mode == ir::MemAccessMode::Signed) {
            value = bit::sign_extend<8, int32_t>(value);
        }
        break;
    case ir::MemAccessSize::Half:
        if (op.mode == ir::MemAccessMode::Signed) {
            if (m_context.GetCPUArch() == CPUArch::ARMv4T) {
                if (addr & 1) {
                    value = bit::sign_extend<8, int32_t>(readByte(addr));
                } else {
                    value = bit::sign_extend<16, int32_t>(readHalf(addr));
                }
            } else {
                value = bit::sign_extend<16, int32_t>(readHalf(addr));
            }
        } else if (op.mode == ir::MemAccessMode::Unaligned) {
            if (m_context.GetCPUArch() == CPUArch::ARMv4T) {
                uint16_t halfValue = readHalf(addr);
                if (addr & 1) {
                    halfValue = std::rotr(halfValue, 8);
                }
                value = halfValue;
            } else {
                value = readHalf(addr);
            }
        } else { // aligned
            value = readHalf(addr);
        }
        break;
    case ir::MemAccessSize::Word:
        value = readWord(addr);
        if (op.mode == ir::MemAccessMode::Unaligned) {
            value = std::rotr(value, (addr & 3) * 8);
        }
        break;
    }
    SetVar(op.dst.var, value);
}

void InterpreterHost::HandleMemWrite(const Op &varOp) {
    auto &op = std::get<ir::IRMemWriteOp>(varOp);
    auto &sys = m_context.GetSystem();
    const auto addr = Get(op.address);
    const auto value = Get(op.src);

    auto &mem = m_memMap.dataWrite;

    auto writeByte = [&](uint32_t addr, uint8_t value) {
        auto *ptr = mem.GetPointer<uint8_t>(addr);
        if (ptr != nullptr) {
            *ptr = value;
        } else {
            sys.MemWriteByte(addr, value);
        }
    };

    auto writeHalf = [&](uint32_t addr, uint16_t value) {
        addr &= ~1;
        auto *ptr = mem.GetPointer<uint16_t>(addr);
        if (ptr != nullptr) {
            *ptr = value;
        } else {
            sys.MemWriteHalf(addr, value);
        }
    };

    auto writeWord = [&](uint32_t addr, uint32_t value) {
        addr &= ~3;
        auto *ptr = mem.GetPointer<uint32_t>(addr);
        if (ptr != nullptr) {
            *ptr = value;
        } else {
            sys.MemWriteWord(addr, value);
        }
    };

    switch (op.size) {
    case ir::MemAccessSize::Byte: writeByte(addr, value); break;
    case ir::MemAccessSize::Half: writeHalf(addr, value); break;
    case ir::MemAccessSize::Word: writeWord(addr, value); break;
    default: util::unreachable();
    }
}

void InterpreterHost::HandlePreload(const Op &varOp) {
    // auto &op = std::get<ir::Ipreload_Op>(varOp);
}

void InterpreterHost::HandleLogicalShiftLeft(const Op &varOp) {
    auto &op = std::get<ir::IRLogicalShiftLeftOp>(varOp);
    const auto value = Get(op.value);
    const auto amount = Get(op.amount);
    const auto [result, carry] = arm::LSL(value, amount);
    SetVar(op.dst.var, result);
    if (op.setCarry && carry) {
        UpdateFlags(m_flags, arm::Flags::C, *carry);
    }
}

void InterpreterHost::HandleLogicalShiftRight(const Op &varOp) {
    auto &op = std::get<ir::IRLogicalShiftRightOp>(varOp);
    const auto value = Get(op.value);
    const auto amount = Get(op.amount);
    const auto [result, carry] = arm::LSR(value, amount);
    SetVar(op.dst.var, result);
    if (op.setCarry && carry) {
        UpdateFlags(m_flags, arm::Flags::C, *carry);
    }
}

void InterpreterHost::HandleArithmeticShiftRight(const Op &varOp) {
    auto &op = std::get<ir::IRArithmeticShiftRightOp>(varOp);
    const auto value = Get(op.value);
    const auto amount = Get(op.amount);
    const auto [result, carry] = arm::ASR(value, amount);
    SetVar(op.dst.var, result);
    if (op.setCarry && carry) {
        UpdateFlags(m_flags, arm::Flags::C, *carry);
    }
}

void InterpreterHost::HandleRotateRight(const Op &varOp) {
    auto &op = std::get<ir::IRRotateRightOp>(varOp);
    const auto value = Get(op.value);
    const auto amount = Get(op.amount);
    const auto [result, carry] = arm::ROR(value, amount);
    SetVar(op.dst.var, result);
    if (op.setCarry && carry) {
        UpdateFlags(m_flags, arm::Flags::C, *carry);
    }
}

void InterpreterHost::HandleRotateRightExtended(const Op &varOp) {
    auto &op = std::get<ir::IRRotateRightExtendedOp>(varOp);
    const auto value = Get(op.value);
    const auto [result, carry] = arm::RRX(value, BitmaskEnum(m_flags).AnyOf(arm::Flags::C));
    SetVar(op.dst.var, result);
    if (op.setCarry) {
        UpdateFlags(m_flags, arm::Flags::C, carry);
    }
}

void InterpreterHost::HandleBitwiseAnd(const Op &varOp) {
    auto &op = std::get<ir::IRBitwiseAndOp>(varOp);
    const auto lhs = Get(op.lhs);
    const auto rhs = Get(op.rhs);
    const auto result = lhs & rhs;
    SetVar(op.dst.var, result);
    UpdateNZ(m_flags, op.flags, result);
}

void InterpreterHost::HandleBitwiseOr(const Op &varOp) {
    auto &op = std::get<ir::IRBitwiseOrOp>(varOp);
    const auto lhs = Get(op.lhs);
    const auto rhs = Get(op.rhs);
    const auto result = lhs | rhs;
    SetVar(op.dst.var, result);
    UpdateNZ(m_flags, op.flags, result);
}

void InterpreterHost::HandleBitwiseXor(const Op &varOp) {
    auto &op = std::get<ir::IRBitwiseXorOp>(varOp);
    const auto lhs = Get(op.lhs);
    const auto rhs = Get(op.rhs);
    const auto result = lhs ^ rhs;
    SetVar(op.dst.var, result);
    UpdateNZ(m_flags, op.flags, result);
}

void InterpreterHost::HandleBitClear(const Op &varOp) {
    auto &op = std::get<ir::IRBitClearOp>(varOp);
    const auto lhs = Get(op.lhs);
    const auto rhs = Get(op.rhs);
    const auto result = lhs & ~rhs;
    SetVar(op.dst.var, result);
    UpdateNZ(m_flags, op.flags, result);
}

void InterpreterHost::HandleCountLeadingZeros(const Op &varOp) {
    auto &op = std::get<ir::IRCountLeadingZerosOp>(varOp);
    SetVar(op.dst.var, std::countl_zero(Get(op.value)));
}

void InterpreterHost::HandleAdd(const Op &varOp) {
    auto &op = std::get<ir::IRAddOp>(varOp);
    const auto lhs = Get(op.lhs);
    const auto rhs = Get(op.rhs);
    const auto [result, carry, overflow] = arm::ADD(lhs, rhs);
    SetVar(op.dst.var, result);
    UpdateNZCV(m_flags, op.flags, result, carry, overflow);
}

void InterpreterHost::HandleAddCarry(const Op &varOp) {
    auto &op = std::get<ir::IRAddCarryOp>(varOp);
    const auto lhs = Get(op.lhs);
    const auto rhs = Get(op.rhs);
    const bool hostCarry = BitmaskEnum(m_flags).AnyOf(arm::Flags::C);
    const auto [result, carry, overflow] = arm::ADC(lhs, rhs, hostCarry);
    SetVar(op.dst.var, result);
    UpdateNZCV(m_flags, op.flags, result, carry, overflow);
}

void InterpreterHost::HandleSubtract(const Op &varOp) {
    auto &op = std::get<ir::IRSubtractOp>(varOp);
    const auto lhs = Get(op.lhs);
    const auto rhs = Get(op.rhs);
    const auto [result, carry, overflow] = arm::SUB(lhs, rhs);
    SetVar(op.dst.var, result);
    UpdateNZCV(m_flags, op.flags, result, carry, overflow);
}

void InterpreterHost::HandleSubtractCarry(const Op &varOp) {
    auto &op = std::get<ir::IRSubtractCarryOp>(varOp);
    const auto lhs = Get(op.lhs);
    const auto rhs = Get(op.rhs);
    const bool hostCarry = BitmaskEnum(m_flags).AnyOf(arm::Flags::C);
    const auto [result, carry, overflow] = arm::SBC(lhs, rhs, hostCarry);
    SetVar(op.dst.var, result);
    UpdateNZCV(m_flags, op.flags, result, carry, overflow);
}

void InterpreterHost::HandleMove(const Op &varOp) {
    auto &op = std::get<ir::IRMoveOp>(varOp);
    const auto value = Get(op.value);
    SetVar(op.dst.var, value);
    UpdateNZ(m_flags, op.flags, value);
}

void InterpreterHost::HandleMoveNegated(const Op &varOp) {
    auto &op = std::get<ir::IRMoveNegatedOp>(varOp);
    const auto value = ~Get(op.value);
    SetVar(op.dst.var, value);
    UpdateNZ(m_flags, op.flags, value);
}

void InterpreterHost::HandleSaturatingAdd(const Op &varOp) {
    auto &op = std::get<ir::IRSaturatingAddOp>(varOp);
    const int64_t lhs = static_cast<int32_t>(Get(op.lhs));
    const int64_t rhs = static_cast<int32_t>(Get(op.rhs));
    const auto [result, saturated] = arm::Saturate(lhs + rhs);
    SetVar(op.dst.var, result);
    if (BitmaskEnum(op.flags).AnyOf(arm::Flags::V)) {
        m_flagQ |= saturated;
    }
}

void InterpreterHost::HandleSaturatingSubtract(const Op &varOp) {
    auto &op = std::get<ir::IRSaturatingSubtractOp>(varOp);
    const int64_t lhs = static_cast<int32_t>(Get(op.lhs));
    const int64_t rhs = static_cast<int32_t>(Get(op.rhs));
    const auto [result, saturated] = arm::Saturate(lhs - rhs);
    SetVar(op.dst.var, result);
    if (BitmaskEnum(op.flags).AnyOf(arm::Flags::V)) {
        m_flagQ |= saturated;
    }
}

void InterpreterHost::HandleMultiply(const Op &varOp) {
    auto &op = std::get<ir::IRMultiplyOp>(varOp);
    if (op.signedMul) {
        const auto lhs = static_cast<int32_t>(Get(op.lhs));
        const auto rhs = static_cast<int32_t>(Get(op.rhs));
        const auto result = lhs * rhs;
        SetVar(op.dst.var, result);
        UpdateNZ(m_flags, op.flags, result);
    } else {
        const auto lhs = Get(op.lhs);
        const auto rhs = Get(op.rhs);
        const auto result = lhs * rhs;
        SetVar(op.dst.var, result);
        UpdateNZ(m_flags, op.flags, result);
    }
}

void InterpreterHost::HandleMultiplyLong(const Op &varOp) {
    auto &op = std::get<ir::IRMultiplyLongOp>(varOp);
    if (op.signedMul) {
        const int64_t lhs = static_cast<int32_t>(Get(op.lhs));
        const int64_t rhs = static_cast<int32_t>(Get(op.rhs));
        int64_t result = lhs * rhs;
        if (op.shiftDownHalf) {
            result >>= 16ll;
        }
        SetVar(op.dstLo.var, result >> 0ull);
        SetVar(op.dstHi.var, result >> 32ull);
        UpdateNZLong(m_flags, op.flags, result);
    } else {
        const uint64_t lhs = Get(op.lhs);
        const uint64_t rhs = Get(op.rhs);
        uint64_t result = lhs * rhs;
        if (op.shiftDownHalf) {
            result >>= 16ull;
        }
        SetVar(op.dstLo.var, result >> 0ull);
        SetVar(op.dstHi.var, result >> 32ull);
        UpdateNZLong(m_flags, op.flags, result);
    }
}

void InterpreterHost::HandleAddLong(const Op &varOp) {
    auto &op = std::get<ir::IRAddLongOp>(varOp);
    auto value64 = [](uint32_t lo, uint32_t hi) -> uint64_t { return (uint64_t)lo | ((uint64_t)hi << 32ull); };
    const auto lhs = value64(Get(op.lhsLo), Get(op.lhsHi));
    const auto rhs = value64(Get(op.rhsLo), Get(op.rhsHi));
    const auto result = lhs + rhs;
    SetVar(op.dstLo.var, result >> 0ull);
    SetVar(op.dstHi.var, result >> 32ull);
    UpdateNZLong(m_flags, op.flags, result);
}

void InterpreterHost::HandleStoreFlags(const Op &varOp) {
    auto &op = std::get<ir::IRStoreFlagsOp>(varOp);
    const auto flags = static_cast<uint32_t>(op.flags);
    const auto values = Get(op.values);
    m_flags = static_cast<arm::Flags>((static_cast<uint32_t>(m_flags) & ~flags) | (values & flags));
}

void InterpreterHost::HandleLoadFlags(const Op &varOp) {
    auto &op = std::get<ir::IRLoadFlagsOp>(varOp);
    const auto flags = static_cast<uint32_t>(op.flags);
    auto value = Get(op.srcCPSR);
    value = (value & ~flags) | (static_cast<uint32_t>(m_flags) & flags);
    SetVar(op.dstCPSR.var, value);
}

void InterpreterHost::HandleLoadStickyOverflow(const Op &varOp) {
    auto &op = std::get<ir::IRLoadStickyOverflowOp>(varOp);
    auto value = Get(op.srcCPSR);
    if (op.setQ && m_flagQ) {
        value |= (1 << 27);
    }
    SetVar(op.dstCPSR.var, value);
}

void InterpreterHost::HandleBranch(const Op &varOp) {
    auto &op = std::get<ir::IRBranchOp>(varOp);
    const uint32_t instrSize = (m_armState.CPSR().t ? sizeof(uint16_t) : sizeof(uint32_t));
    const uint32_t pcOffset = 2 * instrSize;
    const uint32_t addrMask = ~(instrSize - 1);
    const auto addr = Get(op.address);
    m_armState.GPR(arm::GPR::PC) = (addr & addrMask) + pcOffset;
}

void InterpreterHost::HandleBranchExchange(const Op &varOp) {
    auto &op = std::get<ir::IRBranchExchangeOp>(varOp);
    const auto addr = Get(op.address);

    const bool bxOnAddrBit0 = [&] {
        using Mode = ir::IRBranchExchangeOp::ExchangeMode;
        switch (op.bxMode) {
        case Mode::AddrBit0: return true;
        case Mode::L4: {
            if (m_context.GetCPUArch() != CPUArch::ARMv5TE) {
                return true;
            }
            const auto &cp15 = m_context.GetARMState().GetSystemControlCoprocessor();
            if (!cp15.IsPresent()) {
                return true;
            }
            return !cp15.GetControlRegister().value.preARMv5;
        }
        case Mode::CPSRThumbFlag: return false;
        default: return true;
        }
    }();

    const bool thumb = bxOnAddrBit0 ? bit::test<0>(addr) : m_armState.CPSR().t;
    const uint32_t instrSize = thumb ? sizeof(uint16_t) : sizeof(uint32_t);
    const uint32_t pcOffset = 2 * instrSize;
    const uint32_t addrMask = ~(instrSize - 1);
    m_armState.GPR(arm::GPR::PC) = (addr & addrMask) + pcOffset;
    m_armState.CPSR().t = thumb;
}

void InterpreterHost::HandleLoadCopRegister(const Op &varOp) {
    auto &op = std::get<ir::IRLoadCopRegisterOp>(varOp);
    auto &cop = m_armState.GetCoprocessor(op.cpnum);
    if (op.ext) {
        SetVar(op.dstValue.var, cop.LoadExtRegister(op.reg));
    } else {
        SetVar(op.dstValue.var, cop.LoadRegister(op.reg));
    }
}

void InterpreterHost::HandleStoreCopRegister(const Op &varOp) {
    auto &op = std::get<ir::IRStoreCopRegisterOp>(varOp);
    auto &cop = m_armState.GetCoprocessor(op.cpnum);
    if (op.ext) {
        cop.StoreExtRegister(op.reg, Get(op.srcValue));
    } else {
        cop.StoreRegister(op.reg, Get(op.srcValue));
    }
}

void InterpreterHost::HandleConstant(const Op &varOp) {
    auto &op = std::get<ir::IRConstantOp>(varOp);
    SetVar(op.dst.var, op.value);
}

void InterpreterHost::HandleCopyVar(const Op &varOp) {
    auto &op = std::get<ir::IRCopyVarOp>(varOp);
    SetVar(op.dst.var, Get(op.var));
}

void InterpreterHost::HandleGetBaseVectorAddress(const Op &varOp) {
    auto &op = std::get<ir::IRGetBaseVectorAddressOp>(varOp);
    auto &cp15 = m_armState.GetSystemControlCoprocessor();
    const auto value = (cp15.IsPresent() ? cp15.GetControlRegister().baseVectorAddress : 0x00000000);
    SetVar(op.dst.var, value);
}

} // namespace armajitto::interp
