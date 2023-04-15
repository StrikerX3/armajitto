#pragma once

#include "core/allocator.hpp"
#include "core/location_ref.hpp"
#include "defs/variable.hpp"
#include "guest/arm/instructions.hpp"
#include "ir/ops/ir_ops_base.hpp"

#include <cassert>
#include <memory>
#include <type_traits>
#include <vector>

namespace armajitto::ir {

class BasicBlock {
public:
    // true = less translator/optimizer performance, lower memory usage
    // false = faster, but memory is wasted on optimization passes until allocator is released
    static constexpr bool kFreeErasedIROps = false;

    enum class Terminal {
        // Jump to specified LocationRef.
        // Used:
        // - By branches to known addresses
        // - When the condition code changes
        // - The block is terminated by the block size limit
        DirectLink,

        // Lookup block specified by current PC + CPSR mode + CPSR T.
        // Used by branches to unknown addresses.
        IndirectLink,

        // Return to dispatcher (default).
        // Used by exceptions, ALU writes to PC and writes to coprocessors with side-effects.
        Return,
    };

    BasicBlock(memory::Allocator &alloc, LocationRef location)
        : m_alloc(alloc)
        , m_location(location) {}

    /*~BasicBlock() {
        Clear();
    }*/

    LocationRef Location() const {
        return m_location;
    }

    arm::Condition Condition() const {
        return m_cond;
    }

    uint32_t InstructionCount() const {
        return m_instrCount;
    }

    uint64_t PassCycles() const {
        return m_passCycles;
    }

    uint64_t FailCycles() const {
        return m_failCycles;
    }

    IROp *Head() {
        return m_opsHead;
    }

    IROp *Tail() {
        return m_opsTail;
    }

    const IROp *Head() const {
        return m_opsHead;
    }

    const IROp *Tail() const {
        return m_opsTail;
    }

    void Clear() {
        if constexpr (kFreeErasedIROps) {
            IROp *op = m_opsHead;
            while (op != nullptr) {
                IROp *next = op->next;
                m_alloc.Free(op);
                op = next;
            }
        }
        m_opsHead = nullptr;
        m_opsTail = nullptr;
    }

    uint32_t VariableCount() const {
        return m_nextVarID;
    }

    Terminal GetTerminal() const {
        return m_terminal;
    }

    // Valid for Terminal::BranchToKnownAddress and Terminal::ContinueExecution
    LocationRef GetTerminalLocation() const {
        return m_terminalLocation;
    }

    // Returns the location reference to the first instruction after this block
    LocationRef NextLocation() const {
        const uint32_t instrSize = m_location.IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t);
        return {m_location.PC() + InstructionCount() * instrSize, m_location.Mode(), m_location.IsThumbMode()};
    }

private:
    memory::Allocator &m_alloc;

    LocationRef m_location;
    arm::Condition m_cond;

    IROp *m_opsHead = nullptr;
    IROp *m_opsTail = nullptr;
    uint32_t m_instrCount = 0; // Number of ARM/Thumb instructions translated into this block
    uint32_t m_nextVarID = 0;

    uint64_t m_passCycles = 0; // Number of cycles taken if the block is executed (condition passes)
    uint64_t m_failCycles = 0; // Number of cycles taken if the block is skipped (condition fails)

    Terminal m_terminal = Terminal::Return;
    LocationRef m_terminalLocation{};

    // -------------------------------------------------------------------------
    // Emitter accessors
    // Allows modification of the IR code inside the block

    friend class Emitter;

    void NextInstruction() {
        ++m_instrCount;
    }

    void SetCondition(arm::Condition cond) {
        m_cond = cond;
    }

    void AddPassCycles(uint64_t cycles) {
        m_passCycles += cycles;
    }

    void AddFailCycles(uint64_t cycles) {
        m_failCycles += cycles;
    }

    template <typename T, typename... Args>
    IROp *CreateOp(Args &&...args) {
        return m_alloc.Allocate<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    IROp *AppendOp(IROp *ref, Args &&...args) {
        IROp *op = CreateOp<T>(std::forward<Args>(args)...);
        if (ref == nullptr) {
            m_opsHead = m_opsTail = op;
        } else {
            ref->Append(op);
            if (ref == m_opsTail) {
                m_opsTail = op;
            }
        }
        return op;
    }

    template <typename T, typename... Args>
    IROp *PrependOp(IROp *ref, Args &&...args) {
        IROp *op = CreateOp<T>(std::forward<Args>(args)...);
        if (ref == nullptr) {
            m_opsHead = m_opsTail = op;
        } else {
            ref->Prepend(op);
            if (ref == m_opsHead) {
                m_opsHead = op;
            }
        }
        return op;
    }

    template <typename T, typename... Args>
    IROp *ReplaceOp(IROp *ref, Args &&...args) {
        IROp *op = CreateOp<T>(std::forward<Args>(args)...);
        if (ref == nullptr) {
            m_opsHead = m_opsTail = op;
        } else {
            ref->Replace(op);
            if (ref == m_opsHead) {
                m_opsHead = op;
            }
            if (ref == m_opsTail) {
                m_opsTail = op;
            }
            if constexpr (kFreeErasedIROps) {
                m_alloc.Free(ref);
            }
        }
        return op;
    }

    void InsertHead(IROp *op) {
        if (m_opsHead == nullptr) {
            m_opsHead = op;
            m_opsTail = op;
        } else {
            m_opsHead->Prepend(op);
            m_opsHead = op;
        }
    }

    void InsertTail(IROp *op) {
        if (m_opsTail == nullptr) {
            m_opsTail = op;
            m_opsHead = op;
        } else {
            m_opsTail->Append(op);
            m_opsTail = op;
        }
    }

    IROp *Detach(IROp *op) {
        if (op == m_opsHead) {
            m_opsHead = m_opsHead->Next();
        }
        if (op == m_opsTail) {
            m_opsTail = nullptr;
        }
        return op->Erase();
    }

    IROp *Erase(IROp *op) {
        IROp *next = Detach(op);
        if constexpr (kFreeErasedIROps) {
            m_alloc.Free(op);
        }
        return next;
    }

    uint32_t NextVarID() {
        return m_nextVarID++;
    }

    void RenameVariables();

    void TerminateDirectLink(LocationRef branchTarget) {
        m_terminal = Terminal::DirectLink;
        m_terminalLocation = branchTarget;
    }

    void TerminateDirectLinkNextBlock() {
        const uint32_t instrSize = (m_location.IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t));
        const uint32_t targetAddress = m_location.PC() + m_instrCount * instrSize;

        m_terminal = Terminal::DirectLink;
        m_terminalLocation = {targetAddress, m_location.Mode(), m_location.IsThumbMode()};
    }

    void TerminateIndirectLink() {
        m_terminal = Terminal::IndirectLink;
    }

    void TerminateReturn() {
        m_terminal = Terminal::Return;
    }
};

static_assert(std::is_trivially_destructible_v<BasicBlock>,
              "BasicBlock must be trivially destructible to be efficiently used with the allocator");

} // namespace armajitto::ir
