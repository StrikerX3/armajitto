#pragma once

#include "system.hpp"

#include <memory>

class Interpreter {
public:
};

std::unique_ptr<Interpreter> MakeARM946ESInterpreter(FuzzerSystem &sys);
