#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Automation);

    /** One command for the runner: what to do, with what, and the id its answer carries. */
    struct AutomationRequest
    {
        std::string    Id;
        std::string    Command;
        nlohmann::json Params = nlohmann::json::object();
    };

    /** A request's answer. */
    struct AutomationResponse
    {
        std::string    Id;
        std::string    Command;
        bool           bOk = false;
        nlohmann::json Result = nlohmann::json::object();   // when ok: what the command returned
        std::string    Error;                               // when not: why
    };

    /** What a command returns. */
    struct AutomationResult
    {
        bool           bOk   = true;
        nlohmann::json Value = nlohmann::json::object();
        std::string    Error;

        /** Frames to let run before the next request (a screenshot taken, a key held, a wait). */
        Uint32 WaitFrames = 0;

        /** After those frames, holds the queue frame after frame until it returns true (a game clock). */
        TFunction<bool()> WaitUntil;

        /** Runs once the wait is over, before the next request (a held key released). */
        TFunction<void()> AfterWait;

        static AutomationResult Ok(nlohmann::json InValue = nlohmann::json::object());
        static AutomationResult Fail(std::string InError);
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
        void Register(const std::string& InName, const std::string& InHelp, AutomationHandler InHandler);

        bool Has(const std::string& InName) const;

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

        Uint64 GetAnswered() const noexcept { return m_Answered; }
        Uint64 GetFailed()   const noexcept { return m_Failed; }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        void Run(const AutomationRequest& InRequest);

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
            std::string       Help;
            AutomationHandler Handler;
        };

        TUnorderedMap<std::string, Command> m_Commands;
        TQueue<AutomationRequest>           m_Queue;

        Uint32            m_WaitFrames = 0;
        TFunction<bool()> m_WaitUntil;
        TFunction<void()> m_AfterWait;

        TFunction<void(const AutomationResponse&)> m_Sink;

        Uint64 m_Answered = 0;
        Uint64 m_Failed   = 0;
    };

    /** A request from its JSON, {"id", "command", "params"}. False (with OutError) when malformed. */
    bool ParseAutomationRequest(const nlohmann::json& InJson, AutomationRequest& OutRequest, std::string& OutError);

    /**
     * The requests of a script: a JSON array of requests, or {"requests": [...]}. A request without
     * an id gets its position. False (with OutError) when malformed.
     */
    bool ParseAutomationScript(const std::string& InText, TDynArray<AutomationRequest>& OutRequests,
                               std::string& OutError);

    nlohmann::json ToJson(const AutomationResponse& InResponse);
}
