#pragma once

#include "basic_block.hpp"
#include "ops/ir_ops_visitor.hpp"

#include <vector>

namespace armajitto::ir {

class Verifier {
public:
    bool Verify(const BasicBlock &block) {
#ifdef _DEBUG
        bool valid = true;
        m_initializedVars.resize(block.VariableCount());
        std::fill(m_initializedVars.begin(), m_initializedVars.end(), false);

        auto *op = block.Head();
        while (op != nullptr) {
            VisitIROpVars(op, [this, &valid](auto *op, Variable var, bool read) {
                if (read) {
                    if (!var.IsPresent()) {
                        // Ensure all required (read) variables are present
                        auto opStr = op->ToString();
                        printf("'%s' reads from an absent variable\n", opStr.c_str());
                        valid = false;
                    } else if (!m_initializedVars[var.Index()]) {
                        // Ensure reads are always from initialized variables
                        auto opStr = op->ToString();
                        auto varStr = var.ToString();
                        printf("'%s' reads from uninitialized variable %s\n", opStr.c_str(), varStr.c_str());
                        valid = false;
                    }
                } else {
                    // Mark variable as initialized
                    m_initializedVars[var.Index()] = true;
                }
            });
            op = op->Next();
        }
        return valid;
#else
        return true;
#endif
    }

private:
#ifdef _DEBUG
    std::vector<bool> m_initializedVars;
#endif
};

} // namespace armajitto::ir
