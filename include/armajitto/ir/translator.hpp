#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/defs/arm/instructions.hpp"
#include "basic_block.hpp"

namespace armajitto::ir {

class Translator {
public:
    Translator(Context &context)
        : m_context(context) {}

    // TODO: move baseAddress into BasicBlock and include arm::Mode and ARM/Thumb state
    // TODO: these methods should be private and selected based on BasicBlock's ARM/Thumb state
    //   add this to the public interface instead:
    //     void Translate(BasicBlock &block, uint32_t maxBlockSize);
    //   if more parameters are needed, use a struct (Translator::Parameters)
    void TranslateARM(BasicBlock &block, uint32_t baseAddress, uint32_t maxBlockSize);
    void TranslateThumb(BasicBlock &block, uint32_t baseAddress, uint32_t maxBlockSize);

private:
    Context &m_context;

    template <typename FetchDecodeFn>
    void TranslateCommon(BasicBlock &block, uint32_t baseAddress, uint32_t maxBlockSize, FetchDecodeFn &&fetchDecodeFn);

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

    void DecodeARM(uint32_t opcode, State &state);
    void DecodeThumb(uint16_t opcode, State &state);

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
