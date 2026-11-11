#pragma once

#include "Application/Services/IAppService.h"
#include "Commands/CommandHandle.h"
#include "Commands/CommandTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // ICommandRegistry — the app's commands, there in every run: automation scripts and the dev
    //   console run them, the engine, the app and the game add them. Game code registers through
    //   its context (GameInstanceContext::Commands, WorldContext::Commands) and keeps the handle as
    //   a member, so the command is gone with its owner.
    // =============================================================================
    class ICommandRegistry : public IAppService
    {
    public:
        OPAAX_SERVICE_TYPE(ICommandRegistry)

        /** Adds a command for the registry's lifetime (the engine's, the app's). */
        virtual void Add(CommandSpec InSpec, CommandHandler InHandler) = 0;

        /** Add with a name and a line of help, and no declared parameters. */
        void Add(const OpaaxString& InName, const OpaaxString& InHelp, CommandHandler InHandler);

        /**
         * Adds a command for the lifetime of the handle it returns: dropping the handle removes it.
         * A name registered again runs the newest; each handle removes only its own.
         */
        [[nodiscard]] virtual CommandHandle Register(CommandSpec InSpec, CommandHandler InHandler) = 0;

        /** The registration of InName that runs (the newest), or null. Kept alive while held. */
        virtual TSharedPtr<const RegisteredCommand> Find(const OpaaxString& InName) const = 0;

        bool Has(const OpaaxString& InName) const;

        /** Every command, sorted by name: [{"name", "help", "params"?}]. */
        virtual nlohmann::json Describe() const = 0;

        //----- null object ----------------------------------------------------
        /** A registry that keeps nothing: Register returns an empty handle, Find finds nothing. */
        static ICommandRegistry& Null();
    };
}
