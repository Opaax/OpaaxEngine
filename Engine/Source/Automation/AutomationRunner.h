#pragma once

#include <nlohmann/json.hpp>

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

    /** What a command returns. */
    struct AutomationResult
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
        TFunction<AutomationResult()> Answer;

        /** With Answer: the answer when the app closes before the wait is over (a failure if unset). */
        TFunction<AutomationResult()> OnClose;

        static AutomationResult Ok(nlohmann::json InValue = nlohmann::json::object());
        static AutomationResult Fail(OpaaxString InError);
    };

    using AutomationHandler = TFunction<AutomationResult(const nlohmann::json& InParams)>;

    // =============================================================================
    // AutomationRunner — drives the app from outside (an AI agent, a test script): requests run
    //   one after another at the start of frames, each answered with a response. A request can
    //   hold the queue for frames (waits, screenshots, held keys). Commands are registered by name
    //   with a line of help, by the engine and the editor; "commands.list" lists them and
    //   "frames.wait" is built in.
    // =============================================================================
    class AutomationRunner
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        AutomationRunner();

        AutomationRunner(const AutomationRunner&)            = delete;
        AutomationRunner& operator=(const AutomationRunner&) = delete;

        // =============================================================================
        // Commands
        // =============================================================================
    public:
        /** Adds the command InName, or replaces it (the editor refines some of the engine's). */
        void Register(const OpaaxString& InName, const OpaaxString& InHelp, AutomationHandler InHandler);

        bool Has(const OpaaxString& InName) const;

        /** Every command and its help, sorted by name: [{"name", "help"}]. */
        nlohmann::json Describe() const;

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
        struct Command
        {
            OpaaxString       Help;
            AutomationHandler Handler;
        };

        TUnorderedMap<OpaaxString, Command> m_Commands;
        TQueue<AutomationRequest>           m_Queue;

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
