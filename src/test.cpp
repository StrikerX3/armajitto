#include <armajitto/armajitto.hpp>

#include <cstdio>

class System : public armajitto::ISystem {
public:
};

int main() {
    printf("armajitto %s\n", armajitto::version::name);

    // System implements the armajitto::ISystem interface
    System sys{/* TODO: args */};

    // Define a specification for the recompiler
    armajitto::Specification spec{
        .system = sys,
        .cpuModel = armajitto::CPUModel::ARMv5TE,
    };

    /*
    // Make a recompiler from the specification
    armajitto::Recompiler jit{spec};

    // Get the ARM state -- registers, coprocessors, etc.
    armajitto::arm::State &armState = jit.ARMState();

    // Convenience method to start execution at the specified address and execution state
    jit.JumpTo(0x2000000, armajitto::arm::ExecState::ARM);

    // Raise the IRQ line
    jit.IRQLine() = true;
    // Interrupts are handled in Run()

    // Run for at least 32 cycles
    uint64_t cyclesExecuted = jit.Run(32);

    // Switch to FIQ mode (also switches banked registers and updates I and F flags)
    armState.SetMode(armajitto::arm::Mode::FIQ);
    */
    return EXIT_SUCCESS;
}
