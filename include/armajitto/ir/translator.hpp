#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/defs/arm/instructions.hpp"
#include "emitter.hpp"

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
                return state.emitter;
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
            : emitter(block)
            , flagsUpdated(false)
            , endBlock(false) {}

        void UpdateCondition(arm::Condition cond) {
            if (!condKnown) {
                emitter.SetCondition(cond);
                condKnown = true;
            } else if (cond != emitter.GetBlock().Condition()) {
                endBlock = true;
            }
        }

        void NextIteration() {
            if (flagsUpdated && emitter.GetBlock().Condition() != arm::Condition::AL) {
                endBlock = true;
            }
            flagsUpdated = false;
        }

        bool IsEndBlock() const {
            return endBlock;
        }

        Handle GetHandle() {
            return Handle{*this};
        }

    private:
        Emitter emitter;
        bool condKnown = false;
        bool flagsUpdated;
        bool endBlock;
    };

    void TranslateARM(uint32_t opcode, State &state);
    void TranslateThumb(uint16_t opcode, State &state);

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
