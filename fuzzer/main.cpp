#include <armajitto/armajitto.hpp>

#include "interp.hpp"
#include "system.hpp"

int main() {
    FuzzerSystem interpSys;
    FuzzerSystem jitSys;

    armajitto::Specification spec{jitSys, armajitto::CPUModel::ARM946ES};
    armajitto::Recompiler jit{spec};

    return 0;
}
