#include "Automation/AutomationRunner.h"

#include <algorithm>

namespace Opaax
{
    namespace
    {
        /** Runs a handler; a parameter of the wrong type fails the request, not the app. */
        AutomationResult Guarded(const TFunction<AutomationResult()>& InCall)
        {
            try
            {
                return InCall();
            }
            catch (const nlohmann::json::exception& lError)
            {
                return AutomationResult::Fail(std::string("bad params: ") + lError.what());
            }
        }

        AutomationResponse MakeResponse(const AutomationRequest& InRequest, AutomationResult InResult)
        {
            AutomationResponse lResponse;
            lResponse.Id      = InRequest.Id;
            lResponse.Command = InRequest.Command;
            lResponse.bOk     = InResult.bOk;
            lResponse.Result  = InResult.bOk ? Move(InResult.Value) : nlohmann::json::object();
            lResponse.Error   = Move(InResult.Error);
            return lResponse;
        }
    }

    // =========================================================================
    // AutomationResult
    // =========================================================================
    AutomationResult AutomationResult::Ok(nlohmann::json InValue)
    {
        AutomationResult lResult;
        lResult.Value = Move(InValue);
        return lResult;
    }

    AutomationResult AutomationResult::Fail(std::string InError)
    {
        AutomationResult lResult;
        lResult.bOk   = false;
        lResult.Error = Move(InError);
        return lResult;
    }

    // =========================================================================
    // AutomationRunner
    // =========================================================================
    AutomationRunner::AutomationRunner()
    {
        Register("commands.list", "Lists every command with its help.",
                 [this](const nlohmann::json&)
                 {
                     return AutomationResult::Ok(nlohmann::json{ { "commands", Describe() } });
                 });

        Register("frames.wait", "Lets {count} frames run (default 1) before the next request.",
                 [](const nlohmann::json& InParams)
                 {
                     AutomationResult lResult;
                     lResult.WaitFrames = std::max(InParams.value("count", 1u), 1u);
                     return lResult;
                 });
    }

    void AutomationRunner::Register(const std::string& InName, const std::string& InHelp, AutomationHandler InHandler)
    {
        m_Commands[InName] = Command{ InHelp, Move(InHandler) };
    }

    bool AutomationRunner::Has(const std::string& InName) const
    {
        return m_Commands.find(InName) != m_Commands.end();
    }

    nlohmann::json AutomationRunner::Describe() const
    {
        TDynArray<std::string> lNames;
        lNames.reserve(m_Commands.size());
        for (const auto& [lName, lCommand] : m_Commands)
        {
            lNames.push_back(lName);
        }
        std::sort(lNames.begin(), lNames.end());

        nlohmann::json lList = nlohmann::json::array();
        for (const std::string& lName : lNames)
        {
            lList.push_back(nlohmann::json{ { "name", lName }, { "help", m_Commands.at(lName).Help } });
        }
        return lList;
    }

    void AutomationRunner::Enqueue(AutomationRequest InRequest)
    {
        m_Queue.push(Move(InRequest));
    }

    void AutomationRunner::Tick()
    {
        if (IsHolding())
        {
            // The frames a request asked for: this tick is one more of them gone by.
            if (m_WaitFrames > 0 && --m_WaitFrames > 0)
            {
                return;
            }

            // Then its condition, checked once a frame.
            if (m_WaitUntil)
            {
                if (!m_WaitUntil())
                {
                    return;
                }
                m_WaitUntil = nullptr;
            }

            FinishWait();
        }

        while (!m_Queue.empty() && !IsHolding())
        {
            const AutomationRequest lRequest = Move(m_Queue.front());
            m_Queue.pop();
            Run(lRequest);
        }
    }

    void AutomationRunner::FinishWait()
    {
        // A request answered now that its wait is over.
        if (m_DeferredAnswer)
        {
            const TFunction<AutomationResult()> lAnswer = Move(m_DeferredAnswer);
            m_DeferredAnswer = nullptr;
            Respond(MakeResponse(m_Deferred, Guarded(lAnswer)));
        }

        if (m_AfterWait)
        {
            const TFunction<void()> lAfter = Move(m_AfterWait);
            m_AfterWait = nullptr;
            lAfter();
        }
    }

    void AutomationRunner::SetResponseSink(TFunction<void(const AutomationResponse&)> InSink)
    {
        m_Sink = Move(InSink);
    }

    bool AutomationRunner::IsIdle() const noexcept
    {
        return m_Queue.empty() && !IsHolding();
    }

    void AutomationRunner::Run(const AutomationRequest& InRequest)
    {
        const auto lIt = m_Commands.find(InRequest.Command);
        if (lIt == m_Commands.end())
        {
            Respond(MakeResponse(InRequest, AutomationResult::Fail(
                "unknown command '" + InRequest.Command + "' (commands.list lists them)")));
            return;
        }

        const AutomationHandler& lHandler = lIt->second.Handler;
        AutomationResult         lResult  = Guarded([&lHandler, &InRequest]() { return lHandler(InRequest.Params); });

        if (lResult.bOk)
        {
            m_WaitFrames = lResult.WaitFrames;
            m_WaitUntil  = Move(lResult.WaitUntil);
            m_AfterWait  = Move(lResult.AfterWait);

            // Answered when the wait is over: FinishWait sends it.
            if (lResult.Answer)
            {
                m_Deferred       = InRequest;
                m_DeferredAnswer = Move(lResult.Answer);
            }
            else
            {
                Respond(MakeResponse(InRequest, Move(lResult)));
            }

            // Nothing to wait for: what comes after the wait happens now.
            if (!IsHolding())
            {
                FinishWait();
            }
            return;
        }

        Respond(MakeResponse(InRequest, Move(lResult)));
    }

    void AutomationRunner::Respond(AutomationResponse InResponse)
    {
        ++m_Answered;
        if (!InResponse.bOk)
        {
            ++m_Failed;
            OPAAX_LOG(LogAutomation, Warn, "Request '{}' ({}) failed: {}", InResponse.Id, InResponse.Command, InResponse.Error);
        }
        else
        {
            OPAAX_LOG(LogAutomation, Info, "Request '{}' ({}) done", InResponse.Id, InResponse.Command);
        }

        if (m_Sink)
        {
            m_Sink(InResponse);
        }
    }

    // =========================================================================
    // Parsing
    // =========================================================================
    bool ParseAutomationRequest(const nlohmann::json& InJson, AutomationRequest& OutRequest, std::string& OutError)
    {
        if (!InJson.is_object())
        {
            OutError = "a request is an object: {\"command\": ..., \"params\": {...}}";
            return false;
        }

        const auto lCommand = InJson.find("command");
        if (lCommand == InJson.end() || !lCommand->is_string() || lCommand->get<std::string>().empty())
        {
            OutError = "a request needs a \"command\" string";
            return false;
        }

        OutRequest.Command = lCommand->get<std::string>();

        const auto lId = InJson.find("id");
        if (lId != InJson.end())
        {
            OutRequest.Id = lId->is_string() ? lId->get<std::string>() : lId->dump();
        }

        const auto lParams = InJson.find("params");
        if (lParams != InJson.end())
        {
            if (!lParams->is_object())
            {
                OutError = "\"params\" must be an object";
                return false;
            }
            OutRequest.Params = *lParams;
        }

        return true;
    }

    bool ParseAutomationScript(const std::string& InText, TDynArray<AutomationRequest>& OutRequests, std::string& OutError)
    {
        OutRequests.clear();

        const nlohmann::json lJson = nlohmann::json::parse(InText, nullptr, /*allow_exceptions*/ false);
        if (lJson.is_discarded())
        {
            OutError = "not valid JSON";
            return false;
        }

        const nlohmann::json* lList = &lJson;
        if (lJson.is_object())
        {
            const auto lRequests = lJson.find("requests");
            if (lRequests == lJson.end())
            {
                OutError = "a script is an array of requests, or {\"requests\": [...]}";
                return false;
            }
            lList = &*lRequests;
        }

        if (!lList->is_array())
        {
            OutError = "a script's requests are an array";
            return false;
        }

        for (Uint64 lIndex = 0; lIndex < lList->size(); ++lIndex)
        {
            AutomationRequest lRequest;
            std::string       lError;
            if (!ParseAutomationRequest((*lList)[lIndex], lRequest, lError))
            {
                OutError = "request " + std::to_string(lIndex) + ": " + lError;
                OutRequests.clear();
                return false;
            }

            if (lRequest.Id.empty())
            {
                lRequest.Id = std::to_string(lIndex);
            }
            OutRequests.push_back(Move(lRequest));
        }

        return true;
    }

    nlohmann::json ToJson(const AutomationResponse& InResponse)
    {
        nlohmann::json lJson{ { "id", InResponse.Id }, { "command", InResponse.Command }, { "ok", InResponse.bOk } };
        if (InResponse.bOk)
        {
            lJson["result"] = InResponse.Result;
        }
        else
        {
            lJson["error"] = InResponse.Error;
        }
        return lJson;
    }
}
