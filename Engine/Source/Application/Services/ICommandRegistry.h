#pragma once

#include "Application/Services/IAppService.h"
#include "Commands/CommandEvents.h"
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
    //   A command marked Cheat sets the cheat flag when it answers ok; one marked DevOnly is not
    //   registered in a ship build.
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

        /** Every command, sorted by name: [{"name", "help", "params"?, "cheat"?, "devOnly"?}]. */
        virtual nlohmann::json Describe() const = 0;

        /** Every command's spec, the one that runs for each name, sorted by name (help, completion). */
        virtual TDynArray<CommandSpec> GetSpecs() const = 0;

        // =============================================================================
        // Cheats
        // =============================================================================

        /**
         * Told by whoever ran InCommand (a runner) that it answered ok. A cheat sets the flag and
         * is sent to the cheat listener, at once.
         */
        virtual void NotifySucceeded(const RegisteredCommand& InCommand) = 0;

        /** A cheat answered ok since the flag was last cleared (the game started, or ClearCheatsRun). */
        virtual bool HaveCheatsRun() const = 0;

        /** Clears the flag: a new run starts clean. The engine clears it at every StartGame. */
        virtual void ClearCheatsRun() = 0;

        /** Told each cheat that answers ok (the app publishes it on the engine bus). */
        virtual void SetCheatListener(TFunction<void(const CheatUsed&)> InListener) = 0;

        //----- null object ----------------------------------------------------
        /** A registry that keeps nothing: Register returns an empty handle, Find finds nothing. */
        static ICommandRegistry& Null();
    };
}
