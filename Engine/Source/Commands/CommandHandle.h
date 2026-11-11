#pragma once

#include "Commands/CommandTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct CommandRegistryState;

    /** A registered command: what it declares and what runs it. */
    struct RegisteredCommand
    {
        CommandSpec    Spec;
        CommandHandler Handler;
        Uint64         Id = 0;
    };

    // =============================================================================
    // CommandHandle — owns one registration: destroying or resetting it removes that command.
    //   Moves, never copies. Safe to outlive its registry (it then removes nothing).
    // =============================================================================
    class CommandHandle
    {
    public:
        CommandHandle() = default;
        ~CommandHandle();

        CommandHandle(CommandHandle&& InOther) noexcept;
        CommandHandle& operator=(CommandHandle&& InOther) noexcept;

        CommandHandle(const CommandHandle&)            = delete;
        CommandHandle& operator=(const CommandHandle&) = delete;

        /** Removes the command it owns, if any. */
        void Reset();

        /** It owns a registration. */
        bool IsValid() const noexcept { return m_State != nullptr; }

    private:
        friend class CommandRegistry;
        CommandHandle(TSharedPtr<CommandRegistryState> InState, OpaaxString InName, Uint64 InId);

        TSharedPtr<CommandRegistryState> m_State;
        OpaaxString                      m_Name;
        Uint64                           m_Id = 0;
    };
}
