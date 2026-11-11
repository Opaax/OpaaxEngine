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

    // =============================================================================
    // CommandRegistry — every command by name, for whoever runs them (automation scripts, the
    //   console). A name can be registered more than once: the newest registration runs, and
    //   removing one leaves the others (a level change registers the new world's command before
    //   the old world's handle dies). "commands.list" and "frames.wait" are built in.
    // =============================================================================
    class CommandRegistry
    {
    public:
        CommandRegistry();
        ~CommandRegistry();

        CommandRegistry(const CommandRegistry&)            = delete;
        CommandRegistry& operator=(const CommandRegistry&) = delete;

        /** Adds a command for the registry's lifetime (the engine's, the editor's). */
        void Add(CommandSpec InSpec, CommandHandler InHandler);

        /** Add with a name and a line of help, and no declared parameters. */
        void Add(const OpaaxString& InName, const OpaaxString& InHelp, CommandHandler InHandler);

        /** Adds a command for the lifetime of the handle it returns: dropping the handle removes it. */
        [[nodiscard]] CommandHandle Register(CommandSpec InSpec, CommandHandler InHandler);

        /** The registration of InName that runs (the newest), or null. Kept alive while held. */
        TSharedPtr<const RegisteredCommand> Find(const OpaaxString& InName) const;

        bool Has(const OpaaxString& InName) const;

        /** Every command, sorted by name: [{"name", "help", "params"?}]. */
        nlohmann::json Describe() const;

    private:
        Uint64 Insert(CommandSpec InSpec, CommandHandler InHandler);

        TSharedPtr<CommandRegistryState> m_State;
    };
}
