#pragma once

#include "Application/Services/ICommandRegistry.h"
#include "Commands/CommandHandle.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // CommandRegistry — every command by name, for whoever runs them (automation scripts, the
    //   console). A name can be registered more than once: the newest registration runs, and
    //   removing one leaves the others (a level change registers the new world's command before
    //   the old world's handle dies). "commands.list", "commands.has" and "frames.wait" are built in.
    // =============================================================================
    class CommandRegistry final : public ICommandRegistry
    {
    public:
        CommandRegistry();
        ~CommandRegistry() override;

        CommandRegistry(const CommandRegistry&)            = delete;
        CommandRegistry& operator=(const CommandRegistry&) = delete;

        //~Begin ICommandRegistry interface
    public:
        using ICommandRegistry::Add;
        void                                Add(CommandSpec InSpec, CommandHandler InHandler) override;
        [[nodiscard]] CommandHandle         Register(CommandSpec InSpec, CommandHandler InHandler) override;
        TSharedPtr<const RegisteredCommand> Find(const OpaaxString& InName) const override;
        nlohmann::json                      Describe() const override;
        //~End ICommandRegistry interface

    private:
        TSharedPtr<CommandRegistryState> m_State;
    };
}
