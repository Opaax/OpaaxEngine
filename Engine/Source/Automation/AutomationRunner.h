#pragma once

#include <nlohmann/json.hpp>

#include "Commands/CommandRegistry.h"
#include "Core/Hash/OpaaxHash.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Automation);

    /** One command for the runner: what to do, with what, and the id its answer carries. */
    struct AutomationRequest
    {
        OpaaxString    Id;
        OpaaxString    Command;
        nlohmann::json Params = nlohmann::json::object();

        /** The command should refuse: the request passes when it fails, and fails when it does not. */
        bool bExpectFailure = false;

        /** What the answer should hold (null: anything). See MatchesExpected. */
        nlohmann::json Expect;
    };

    /** A request's answer. */
    struct AutomationResponse
    {
        OpaaxString    Id;
        OpaaxString    Command;
        bool           bOk = false;
        nlohmann::json Result = nlohmann::json::object();   // when ok: what the command returned
        OpaaxString    Error;                               // when not: why
    };

    /** The automation names of a command's result and handler (Commands/CommandTypes.h). */
    using AutomationResult  = CommandResult;
    using AutomationHandler = CommandHandler;

    // =============================================================================
    // AutomationRunner — drives the app from outside (an AI agent, a test script): requests run
    //   one after another at the start of frames, each answered with a response. A request can
    //   hold the queue for frames (waits, screenshots, held keys). The commands are those of a
    //   CommandRegistry; each runner has its own queue over it.
    // =============================================================================
    class AutomationRunner
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        explicit AutomationRunner(CommandRegistry& InCommands);

        AutomationRunner(const AutomationRunner&)            = delete;
        AutomationRunner& operator=(const AutomationRunner&) = delete;

        // =============================================================================
        // Commands
        // =============================================================================
    public:
        /**
         * Adds the command InName to the registry for its lifetime. A name registered again runs
         * the newest (the editor refines some of the engine's).
         */
        void Register(const OpaaxString& InName, const OpaaxString& InHelp, AutomationHandler InHandler);

        bool Has(const OpaaxString& InName) const;

        /** Every command and its help, sorted by name: [{"name", "help", "params"?}]. */
        nlohmann::json Describe() const;

        CommandRegistry& GetCommands() noexcept { return m_Commands; }

        // =============================================================================
        // Requests
        // =============================================================================
    public:
        void Enqueue(AutomationRequest InRequest);

        /** Runs requests until one holds the queue or none is left. Called at the start of each frame. */
        void Tick();

        /** Told every response, in order. */
        void SetResponseSink(TFunction<void(const AutomationResponse&)> InSink);

        /** Nothing queued and nothing held. */
        bool IsIdle() const noexcept;

        /**
         * The app is closing: the request waiting for its answer gets it (its OnClose, or a failure),
         * and every request not run yet fails. Called once, after the last frame.
         */
        void Close();

        Uint64 GetAnswered() const noexcept { return m_Answered; }
        Uint64 GetFailed()   const noexcept { return m_Failed; }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        void Run(const AutomationRequest& InRequest);

        /** Counts, logs and sends a response. */
        void Respond(AutomationResponse InResponse);

        /** A request's wait is not over. */
        bool IsHolding() const noexcept { return m_WaitFrames > 0 || m_WaitUntil != nullptr; }

        /** Runs what comes after the wait, once. */
        void FinishWait();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        CommandRegistry&          m_Commands;
        TQueue<AutomationRequest> m_Queue;

        Uint32            m_WaitFrames = 0;
        TFunction<bool()> m_WaitUntil;
        TFunction<void()> m_AfterWait;

        /** A request answered once its wait is over: who it was, and how to answer (or on close). */
        AutomationRequest             m_Deferred;
        TFunction<AutomationResult()> m_DeferredAnswer;
        TFunction<AutomationResult()> m_DeferredOnClose;

        TFunction<void(const AutomationResponse&)> m_Sink;

        Uint64 m_Answered = 0;
        Uint64 m_Failed   = 0;
    };

    /**
     * Whether InActual holds what InExpected names: an object's keys (others are ignored), an array
     * item by item (same length), numbers within 1e-4, anything else equal. OutWhy says where they
     * differ ("maps/1/focused is false, expected true").
     */
    bool MatchesExpected(const nlohmann::json& InExpected, const nlohmann::json& InActual, OpaaxString& OutWhy);

    /**
     * A request from its JSON, {"id", "command", "params", "expectFailure", "expect"}. False (with
     * OutError) when malformed.
     */
    bool ParseAutomationRequest(const nlohmann::json& InJson, AutomationRequest& OutRequest, OpaaxString& OutError);

    /**
     * The requests of a script: a JSON array of requests, or {"requests": [...]}. An item
     * {"repeat": [{"tag": "60"}, ...], "requests": [...]} stands for its requests once per entry, "{tag}"
     * in their strings replaced by the entry's value (a string that is exactly "{tag}" takes the value
     * itself, a number staying a number). A request without an id gets its position. False (with
     * OutError) when malformed.
     */
    bool ParseAutomationScript(const OpaaxString& InText, TDynArray<AutomationRequest>& OutRequests,
                               OpaaxString& OutError);

    nlohmann::json ToJson(const AutomationResponse& InResponse);
}
