#include "interp.hpp"
#include "interp/arm946es.hpp"

using namespace armajitto;

class ARM946ESInterpreter final : public Interpreter {
public:
    ARM946ESInterpreter(FuzzerSystem &sys)
        : m_interp(sys) {}
    ~ARM946ESInterpreter() final = default;

    void Reset() {
        m_interp.Reset();
        m_irqLine = false;
    }

    void JumpTo(uint32_t address, bool thumb) {
        GPR(arm::GPR::PC) = address;
        m_interp.GetRegisters().cpsr.t = thumb;
        m_interp.FillPipeline();
    }

    void Run(uint64_t numCycles) final {
        for (uint64_t cycles = 0; cycles < numCycles; cycles++) {
            if (m_irqLine) {
                if (!m_interp.GetRegisters().cpsr.i) {
                    m_interp.HandleIRQ();
                    return;
                }
            }
            m_interp.Run();
        }
    }

    bool &IRQLine() final {
        return m_irqLine;
    }

    uint32_t &GPR(arm::GPR gpr) final {
        return GPR(gpr, static_cast<arm::Mode>(m_interp.GetRegisters().cpsr.mode));
    }

    uint32_t &GPR(arm::GPR gpr, arm::Mode mode) final {
        auto currModeBank = interp::arm::GetBankFromMode(m_interp.GetRegisters().cpsr.mode);
        auto argModeBank = interp::arm::GetBankFromMode(static_cast<interp::arm::Mode>(mode));
        if (currModeBank == argModeBank) {
            return m_interp.GetRegisters().regs[static_cast<size_t>(gpr)];
        }

        interp::arm::Bank bank;
        switch (gpr) {
        case arm::GPR::R0:
        case arm::GPR::R1:
        case arm::GPR::R2:
        case arm::GPR::R3:
        case arm::GPR::R4:
        case arm::GPR::R5:
        case arm::GPR::R6:
        case arm::GPR::R7:
        case arm::GPR::R15: return m_interp.GetRegisters().regs[static_cast<size_t>(gpr)];

        case arm::GPR::R8:
        case arm::GPR::R9:
        case arm::GPR::R10:
        case arm::GPR::R11:
        case arm::GPR::R12:
            if ((mode == arm::Mode::FIQ && currModeBank == interp::arm::Bank_FIQ) ||
                (mode != arm::Mode::FIQ && currModeBank != interp::arm::Bank_FIQ)) {
                return m_interp.GetRegisters().regs[static_cast<size_t>(gpr)];
            } else if (mode == arm::Mode::FIQ) {
                return m_interp.GetRegisters().bankregs[interp::arm::Bank_FIQ][static_cast<size_t>(gpr) - 8];
            } else {
                bank = interp::arm::Bank_User;
            }
            break;
        case arm::GPR::R13:
        case arm::GPR::R14:
            switch (mode) {
            case arm::Mode::User: bank = interp::arm::Bank_User; break;
            case arm::Mode::FIQ: bank = interp::arm::Bank_FIQ; break;
            case arm::Mode::Supervisor: bank = interp::arm::Bank_Supervisor; break;
            case arm::Mode::Abort: bank = interp::arm::Bank_Abort; break;
            case arm::Mode::IRQ: bank = interp::arm::Bank_IRQ; break;
            case arm::Mode::Undefined: bank = interp::arm::Bank_Undefined; break;
            case arm::Mode::System: bank = interp::arm::Bank_User; break;
            default:
                // invalid mode argument
                assert(false);
                bank = interp::arm::Bank_User;
                break;
            }
            break;
        default:
            // unreachable (normally)
            assert(false);
            bank = interp::arm::Bank_User;
            break;
        }

        return m_interp.GetRegisters().bankregs[bank][static_cast<size_t>(gpr) - 8];
    }

    uint32_t GetCPSR() const final {
        return m_interp.GetRegisters().cpsr.u32;
    }

    void SetCPSR(uint32_t value) final {
        m_interp.GetRegisters().cpsr.u32 = value;
        m_interp.SetMode(m_interp.GetRegisters().cpsr.mode);
    }

    uint32_t GetSPSR() const final {
        auto bank = GetBankFromMode(m_interp.GetRegisters().cpsr.mode);
        return m_interp.GetRegisters().spsr[bank].u32;
    }

    uint32_t GetSPSR(armajitto::arm::Mode mode) const final {
        auto bank = GetBankFromMode(static_cast<interp::arm::Mode>(mode));
        return m_interp.GetRegisters().spsr[bank].u32;
    }

    void SetSPSR(arm::Mode mode, uint32_t value) final {
        auto bank = GetBankFromMode(static_cast<interp::arm::Mode>(mode));
        m_interp.GetRegisters().spsr[bank].u32 = value;
    }

private:
    interp::arm946es::ARM946ES<FuzzerSystem> m_interp;
    bool m_irqLine = false;
};

std::unique_ptr<Interpreter> MakeARM946ESInterpreter(FuzzerSystem &sys) {
    return std::make_unique<ARM946ESInterpreter>(sys);
}
