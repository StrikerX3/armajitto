#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/defs/arm/instructions.hpp"
#include "basic_block.hpp"

namespace armajitto::ir {

class Translator {
public:
    struct Parameters {
        uint32_t maxBlockSize;
    };

    Translator(Context &context)
        : m_context(context) {}

    void Translate(BasicBlock &block, Parameters params);

private:
    Context &m_context;

    struct State {
        struct Handle {
            Handle(State &state)
                : state(state) {}

            Emitter &GetEmitter() {
                return state.codeFrag->emitter;
            }

            void EndBlock() {
                state.endBlock = true;
            }

            void MarkFlagsUpdated() {
                state.flagsUpdated = true;
            }

        private:
            State &state;
        };

        State(BasicBlock &block)
            : block(block)
            , codeFrag(nullptr)
            , flagsUpdated(false)
            , endBlock(false) {}

        void UpdateCondition(arm::Condition cond) {
            currCond = cond;
            if (codeFrag == nullptr || cond != codeFrag->cond) {
                NewCodeFragment();
            }
        }

        bool IsEndBlock() const {
            return endBlock;
        }

        bool IsFlagsUpdated() const {
            return flagsUpdated;
        }

        void NextIteration() {
            if (flagsUpdated && currCond != arm::Condition::AL) {
                NewCodeFragment();
            }
            flagsUpdated = false;
        }

        Handle GetHandle() {
            return Handle{*this};
        }

    private:
        BasicBlock &block;
        IRCodeFragment *codeFrag;
        arm::Condition currCond;
        bool flagsUpdated;
        bool endBlock;

        void NewCodeFragment() {
            codeFrag = block.CreateCodeFragment();
            codeFrag->cond = currCond;
        }
    };

    void TranslateARM(uint32_t address, State &state);
    void TranslateThumb(uint32_t address, State &state);

    void Translate(const arm::instrs::Branch &instr, State::Handle state);
    void Translate(const arm::instrs::BranchAndExchange &instr, State::Handle state);
    void Translate(const arm::instrs::ThumbLongBranchSuffix &instr, State::Handle state);
    void Translate(const arm::instrs::DataProcessing &instr, State::Handle state);
    void Translate(const arm::instrs::CountLeadingZeros &instr, State::Handle state);
    void Translate(const arm::instrs::SaturatingAddSub &instr, State::Handle state);
    void Translate(const arm::instrs::MultiplyAccumulate &instr, State::Handle state);
    void Translate(const arm::instrs::MultiplyAccumulateLong &instr, State::Handle state);
    void Translate(const arm::instrs::SignedMultiplyAccumulate &instr, State::Handle state);
    void Translate(const arm::instrs::SignedMultiplyAccumulateWord &instr, State::Handle state);
    void Translate(const arm::instrs::SignedMultiplyAccumulateLong &instr, State::Handle state);
    void Translate(const arm::instrs::PSRRead &instr, State::Handle state);
    void Translate(const arm::instrs::PSRWrite &instr, State::Handle state);
    void Translate(const arm::instrs::SingleDataTransfer &instr, State::Handle state);
    void Translate(const arm::instrs::HalfwordAndSignedTransfer &instr, State::Handle state);
    void Translate(const arm::instrs::BlockTransfer &instr, State::Handle state);
    void Translate(const arm::instrs::SingleDataSwap &instr, State::Handle state);
    void Translate(const arm::instrs::SoftwareInterrupt &instr, State::Handle state);
    void Translate(const arm::instrs::SoftwareBreakpoint &instr, State::Handle state);
    void Translate(const arm::instrs::Preload &instr, State::Handle state);
    void Translate(const arm::instrs::CopDataOperations &instr, State::Handle state);
    void Translate(const arm::instrs::CopDataTransfer &instr, State::Handle state);
    void Translate(const arm::instrs::CopRegTransfer &instr, State::Handle state);
    void Translate(const arm::instrs::CopDualRegTransfer &instr, State::Handle state);
    void Translate(const arm::instrs::Undefined &instr, State::Handle state);
};

} // namespace armajitto::ir
