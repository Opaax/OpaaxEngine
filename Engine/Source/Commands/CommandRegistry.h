#pragma once

#include "Application/Services/ICommandRegistry.h"
#include "Commands/CommandHandle.h"
#include "Core/Build/BuildInfo.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // CommandRegistry — every command by name, for whoever runs them (automation scripts, the
    //   console). A name can be registered more than once: the newest registration runs, and
    //   removing one leaves the others (a level change registers the new world's command before
    //   the old world's handle dies). Built in: commands.list, commands.has, cheats.info and
    //   frames.wait. Built as a ship build, it refuses DevOnly commands.
    // =============================================================================
    class CommandRegistry final : public ICommandRegistry
    {
    public:
        /** @param bInDevBuild False: a ship build's registry, which refuses DevOnly commands */
        explicit CommandRegistry(bool bInDevBuild = Build::IsDevBuild());
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
        TDynArray<CommandSpec>              GetSpecs() const override;
        bool                                AllowsDevOnly() const override { return m_bDevBuild; }
        void                                NotifySucceeded(const RegisteredCommand& InCommand) override;
        bool                                HaveCheatsRun() const override { return m_bCheatsRun; }
        void                                ClearCheatsRun() override { m_bCheatsRun = false; }
        void                                SetCheatListener(TFunction<void(const CheatUsed&)> InListener) override;
        //~End ICommandRegistry interface

    private:
        /** A DevOnly command in a ship build: not registered. */
        bool IsRefused(const CommandSpec& InSpec) const noexcept { return InSpec.IsDevOnly() && !m_bDevBuild; }

        TSharedPtr<CommandRegistryState> m_State;
        bool                             m_bDevBuild  = true;
        bool                             m_bCheatsRun = false;
        TFunction<void(const CheatUsed&)> m_CheatListener;
    };
}
