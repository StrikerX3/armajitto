#include "interp.hpp"
#include "interp/arm946es.hpp"

class ARM946ESInterpreter final : public Interpreter {
public:
    ARM946ESInterpreter(FuzzerSystem &sys)
        : m_interp(sys) {}

private:
    interp::arm946es::ARM946ES<FuzzerSystem, interp::arm946es::CachedExecutor<FuzzerSystem>> m_interp;
};

std::unique_ptr<Interpreter> MakeARM946ESInterpreter(FuzzerSystem &sys) {
    return std::make_unique<ARM946ESInterpreter>(sys);
}
