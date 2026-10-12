#include "Automation/AutomationRunner.h"

#include <algorithm>
#include "Core/Maths/Maths.h"

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

        /** MatchesExpected below a path: InPath names where InActual is ("" for the whole answer). */
        bool MatchesAt(const nlohmann::json& InExpected, const nlohmann::json& InActual, const std::string& InPath,
                       std::string& OutWhy)
        {
            const auto lWhere = [&InPath]() { return InPath.empty() ? std::string("the answer") : InPath; };

            if (InExpected.is_object())
            {
                if (!InActual.is_object())
                {
                    OutWhy = lWhere() + " is " + InActual.dump() + ", expected an object";
                    return false;
                }
                for (const auto& [lKey, lValue] : InExpected.items())
                {
                    const std::string lPath = InPath.empty() ? lKey : InPath + "/" + lKey;
                    const auto        lIt   = InActual.find(lKey);
                    if (lIt == InActual.end())
                    {
                        OutWhy = lPath + " is missing";
                        return false;
                    }
                    if (!MatchesAt(lValue, *lIt, lPath, OutWhy))
                    {
                        return false;
                    }
                }
                return true;
            }

            if (InExpected.is_array())
            {
                if (!InActual.is_array() || InActual.size() != InExpected.size())
                {
                    OutWhy = lWhere() + " is " + InActual.dump() + ", expected " + std::to_string(InExpected.size())
                           + " item(s)";
                    return false;
                }
                for (Uint64 lIndex = 0; lIndex < InExpected.size(); ++lIndex)
                {
                    const std::string lPath = InPath.empty() ? std::to_string(lIndex) : InPath + "/" + std::to_string(lIndex);
                    if (!MatchesAt(InExpected[lIndex], InActual[lIndex], lPath, OutWhy))
                    {
                        return false;
                    }
                }
                return true;
            }

            const bool bSame = (InExpected.is_number() && InActual.is_number())
                                   ? Maths::Abs(InExpected.get<double>() - InActual.get<double>()) <= 1e-4
                                   : InExpected == InActual;
            if (!bSame)
            {
                OutWhy = lWhere() + " is " + InActual.dump() + ", expected " + InExpected.dump();
            }
            return bSame;
        }

        /**
         * The answer to a request, from what its command returned. A request that expects a failure
         * passes with what the command refused ({"refused": why}), and fails when it did not refuse.
         * One with an "expect" fails when the answer does not hold what it names.
         */
        AutomationResponse MakeResponse(const AutomationRequest& InRequest, AutomationResult InResult)
        {
            AutomationResponse lResponse;
            lResponse.Id      = InRequest.Id;
            lResponse.Command = InRequest.Command;

            if (InRequest.bExpectFailure)
            {
                lResponse.bOk = !InResult.bOk;
                if (lResponse.bOk)
                {
                    lResponse.Result = nlohmann::json{ { "refused", Move(InResult.Error) } };
                }
                else
                {
                    lResponse.Error = "expected to fail, but it succeeded";
                }
                return lResponse;
            }

            std::string lWhy;
            if (InResult.bOk && !InRequest.Expect.is_null() && !MatchesExpected(InRequest.Expect, InResult.Value, lWhy))
            {
                lResponse.Error = "the answer differs: " + lWhy;
                return lResponse;
            }

            lResponse.bOk    = InResult.bOk;
            lResponse.Result = InResult.bOk ? Move(InResult.Value) : nlohmann::json::object();
            lResponse.Error  = Move(InResult.Error);
            return lResponse;
        }

        /** A request whose command never answered (unknown, or the app closed): a failure, expected or not. */
        AutomationResponse MakeFailure(const AutomationRequest& InRequest, std::string InError)
        {
            AutomationResponse lResponse;
            lResponse.Id      = InRequest.Id;
            lResponse.Command = InRequest.Command;
            lResponse.Error   = Move(InError);
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
                     lResult.WaitFrames = Maths::Max(InParams.value("count", 1u), 1u);
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
            m_DeferredAnswer  = nullptr;
            m_DeferredOnClose = nullptr;
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

    void AutomationRunner::Close()
    {
        if (m_DeferredAnswer)
        {
            const TFunction<AutomationResult()> lOnClose = Move(m_DeferredOnClose);
            m_DeferredAnswer  = nullptr;
            m_DeferredOnClose = nullptr;
            Respond(lOnClose ? MakeResponse(m_Deferred, Guarded(lOnClose))
                             : MakeFailure(m_Deferred, "the app closed before this request finished"));
        }

        // The wait ends with the app: what would come after it (a held key's release) has no frame to run in.
        m_WaitFrames = 0;
        m_WaitUntil  = nullptr;
        m_AfterWait  = nullptr;

        while (!m_Queue.empty())
        {
            const AutomationRequest lRequest = Move(m_Queue.front());
            m_Queue.pop();
            Respond(MakeFailure(lRequest, "the app closed before this request ran"));
        }
    }

    void AutomationRunner::Run(const AutomationRequest& InRequest)
    {
        const auto lIt = m_Commands.find(InRequest.Command);
        if (lIt == m_Commands.end())
        {
            Respond(MakeFailure(InRequest, "unknown command '" + InRequest.Command + "' (commands.list lists them)"));
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
                m_Deferred        = InRequest;
                m_DeferredAnswer  = Move(lResult.Answer);
                m_DeferredOnClose = Move(lResult.OnClose);
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
    // Answers
    // =========================================================================
    bool MatchesExpected(const nlohmann::json& InExpected, const nlohmann::json& InActual, std::string& OutWhy)
    {
        return MatchesAt(InExpected, InActual, std::string(), OutWhy);
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

        const auto lExpectFailure = InJson.find("expectFailure");
        if (lExpectFailure != InJson.end())
        {
            if (!lExpectFailure->is_boolean())
            {
                OutError = "\"expectFailure\" is true or false";
                return false;
            }
            OutRequest.bExpectFailure = lExpectFailure->get<bool>();
        }

        const auto lExpect = InJson.find("expect");
        if (lExpect != InJson.end())
        {
            if (OutRequest.bExpectFailure)
            {
                OutError = "\"expect\" checks an answer and \"expectFailure\" a refusal: a request has one or the other";
                return false;
            }
            OutRequest.Expect = *lExpect;
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
