#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // Results and handlers
    // =============================================================================

    /** What a command returns: a value or an error, and how long it holds its caller's queue. */
    struct CommandResult
    {
        bool           bOk   = true;
        nlohmann::json Value = nlohmann::json::object();
        OpaaxString    Error;

        /** Frames to let run before the next request (a screenshot taken, a key held, a wait). */
        Uint32 WaitFrames = 0;

        /** After those frames, holds the queue frame after frame until it returns true (a game clock). */
        TFunction<bool()> WaitUntil;

        /** Runs once the wait is over, before the next request (a held key released). */
        TFunction<void()> AfterWait;

        /**
         * With a wait: the request is answered once the wait is over, with what this returns (the
         * outcome of a long job), instead of at once.
         */
        TFunction<CommandResult()> Answer;

        /** With Answer: the answer when the app closes before the wait is over (a failure if unset). */
        TFunction<CommandResult()> OnClose;

        static CommandResult Ok(nlohmann::json InValue = nlohmann::json::object());
        static CommandResult Fail(OpaaxString InError);
    };

    /** Runs a command with its parameters (a JSON object). */
    using CommandHandler = TFunction<CommandResult(const nlohmann::json& InParams)>;

    // =============================================================================
    // Parameters
    // =============================================================================

    /** What a parameter holds. Any takes every JSON value (its choices are only hints). */
    enum class ECommandParamType : Uint8
    {
        Any,
        Bool,
        Int,
        Float,
        String
    };

    /** The name of a parameter type, as Describe writes it ("float"). */
    const char* ToString(ECommandParamType InType) noexcept;

    /** One declared parameter of a command. */
    struct CommandParam
    {
        OpaaxString            Name;
        ECommandParamType      Type = ECommandParamType::Any;
        OpaaxString            Help;
        TDynArray<OpaaxString> Choices;
        bool                   bOptional = false;
    };

    // =============================================================================
    // CommandSpec — a command's name, help and parameters, built in a chain:
    //   CommandSpec("bhop.teleport", "Moves the rabbit.")
    //       .Param("x", ECommandParamType::Any, "start, finish or the x position").Choices({ "start", "finish" })
    //       .Param("y", ECommandParamType::Float, "the y position").Optional()
    //   Choices and Optional apply to the parameter declared last.
    // =============================================================================
    class CommandSpec
    {
    public:
        CommandSpec() = default;
        CommandSpec(OpaaxString InName, OpaaxString InHelp);

        /** Declares the next parameter, in the order a typed line fills them. */
        CommandSpec& Param(OpaaxString InName, ECommandParamType InType, OpaaxString InHelp);

        /** The values the last parameter takes (closed for a typed parameter, hints for Any). */
        CommandSpec& Choices(TDynArray<OpaaxString> InChoices);

        /** The last parameter may be left out. */
        CommandSpec& Optional();

        const OpaaxString&             GetName()   const noexcept { return m_Name; }
        const OpaaxString&             GetHelp()   const noexcept { return m_Help; }
        const TDynArray<CommandParam>& GetParams() const noexcept { return m_Params; }

        /** {"name", "help"}, and "params" when it declares some. */
        nlohmann::json ToJson() const;

    private:
        OpaaxString             m_Name;
        OpaaxString             m_Help;
        TDynArray<CommandParam> m_Params;
    };
}
