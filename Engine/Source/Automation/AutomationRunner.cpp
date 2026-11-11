#include "Automation/AutomationRunner.h"

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
                return AutomationResult::Fail(OpaaxString("bad params: ") + lError.what());
            }
        }

        /** MatchesExpected below a path: InPath names where InActual is ("" for the whole answer). */
        bool MatchesAt(const nlohmann::json& InExpected, const nlohmann::json& InActual, const OpaaxString& InPath,
                       OpaaxString& OutWhy)
        {
            const auto lWhere = [&InPath]() { return InPath.IsEmpty() ? OpaaxString("the answer") : InPath; };

            if (InExpected.is_object())
            {
                if (!InActual.is_object())
                {
                    OutWhy = lWhere() + " is " + ToJsonText(InActual) + ", expected an object";
                    return false;
                }
                for (const auto& [lKey, lValue] : InExpected.items())
                {
                    const OpaaxString lKeyText = FromJsonString(lKey);
                    const OpaaxString lPath = InPath.IsEmpty() ? lKeyText : InPath + "/" + lKeyText;
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
                    OutWhy = lWhere() + " is " + ToJsonText(InActual) + ", expected "
                           + OpaaxString::FromUInt(InExpected.size()) + " item(s)";
                    return false;
                }
                for (Uint64 lIndex = 0; lIndex < InExpected.size(); ++lIndex)
                {
                    const OpaaxString lPath = InPath.IsEmpty() ? OpaaxString::FromUInt(lIndex)
                                                               : InPath + "/" + OpaaxString::FromUInt(lIndex);
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
                OutWhy = lWhere() + " is " + ToJsonText(InActual) + ", expected " + ToJsonText(InExpected);
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

            OpaaxString lWhy;
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
        AutomationResponse MakeFailure(const AutomationRequest& InRequest, OpaaxString InError)
        {
            AutomationResponse lResponse;
            lResponse.Id      = InRequest.Id;
            lResponse.Command = InRequest.Command;
            lResponse.Error   = Move(InError);
            return lResponse;
        }
    }

    // =========================================================================
    // AutomationRunner
    // =========================================================================
    AutomationRunner::AutomationRunner(ICommandRegistry& InCommands)
        : m_Commands(InCommands)
    {
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
        // Held while it runs: a handler may remove its own command (a level change).
        const TSharedPtr<const RegisteredCommand> lCommand = m_Commands.Find(InRequest.Command);
        if (lCommand == nullptr)
        {
            Respond(MakeFailure(InRequest, "unknown command '" + InRequest.Command + "' (commands.list lists them)"));
            return;
        }

        AutomationResult lResult = Guarded([&lCommand, &InRequest]() { return lCommand->Handler(InRequest.Params); });

        if (lResult.bOk)
        {
            // A cheat counts once it answered ok, before its wait: what it did is done.
            m_Commands.NotifySucceeded(*lCommand);

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
    bool MatchesExpected(const nlohmann::json& InExpected, const nlohmann::json& InActual, OpaaxString& OutWhy)
    {
        return MatchesAt(InExpected, InActual, OpaaxString(), OutWhy);
    }

    // =========================================================================
    // Parsing
    // =========================================================================
    bool ParseAutomationRequest(const nlohmann::json& InJson, AutomationRequest& OutRequest, OpaaxString& OutError)
    {
        if (!InJson.is_object())
        {
            OutError = "a request is an object: {\"command\": ..., \"params\": {...}}";
            return false;
        }

        const auto lCommand = InJson.find("command");
        if (lCommand == InJson.end() || !lCommand->is_string() || lCommand->get<OpaaxString>().IsEmpty())
        {
            OutError = "a request needs a \"command\" string";
            return false;
        }

        OutRequest.Command = lCommand->get<OpaaxString>();

        const auto lId = InJson.find("id");
        if (lId != InJson.end())
        {
            OutRequest.Id = lId->is_string() ? lId->get<OpaaxString>() : ToJsonText(*lId);
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

    namespace
    {
        /**
         * InValue with "{key}" replaced by InEntry's value for every key of InEntry, in every string it
         * holds. A string that is exactly "{key}" becomes the value itself, so a number stays a number.
         */
        nlohmann::json Substitute(const nlohmann::json& InValue, const nlohmann::json& InEntry)
        {
            if (InValue.is_string())
            {
                OpaaxString lText = InValue.get<OpaaxString>();
                for (const auto& lPair : InEntry.items())
                {
                    const OpaaxString lPlaceholder = "{" + OpaaxString(lPair.key().c_str()) + "}";
                    if (lText == lPlaceholder)
                    {
                        return lPair.value();
                    }

                    const OpaaxString lWith = lPair.value().is_string() ? lPair.value().get<OpaaxString>()
                                                                        : OpaaxString(lPair.value().dump().c_str());
                    lText = lText.Replace(lPlaceholder, lWith);
                }
                return nlohmann::json(lText);
            }

            if (InValue.is_object() || InValue.is_array())
            {
                nlohmann::json lCopy = InValue;
                for (auto& lChild : lCopy)
                {
                    lChild = Substitute(lChild, InEntry);
                }
                return lCopy;
            }

            return InValue;
        }

        /** Appends InItems to OutItems, each repeat replaced by its requests, once per entry. */
        bool ExpandItems(const nlohmann::json& InItems, nlohmann::json& OutItems, OpaaxString& OutError)
        {
            for (const nlohmann::json& lItem : InItems)
            {
                const auto lRepeat = lItem.is_object() ? lItem.find("repeat") : lItem.end();
                if (!lItem.is_object() || lRepeat == lItem.end())
                {
                    OutItems.push_back(lItem);
                    continue;
                }

                const auto lRequests = lItem.find("requests");
                if (!lRepeat->is_array() || lRequests == lItem.end() || !lRequests->is_array())
                {
                    OutError = "a repeat is {\"repeat\": [{\"tag\": \"60\"}, ...], \"requests\": [...]}: its requests "
                               "once per entry, \"{tag}\" in their strings replaced";
                    return false;
                }

                for (const nlohmann::json& lEntry : *lRepeat)
                {
                    if (!lEntry.is_object())
                    {
                        OutError = "each entry of a repeat is an object of names and values, like {\"tag\": \"60\"}";
                        return false;
                    }

                    // A repeat inside is expanded with this entry's values already in.
                    if (!ExpandItems(Substitute(*lRequests, lEntry), OutItems, OutError))
                    {
                        return false;
                    }
                }
            }
            return true;
        }
    }

    bool ParseAutomationScript(const OpaaxString& InText, TDynArray<AutomationRequest>& OutRequests, OpaaxString& OutError)
    {
        OutRequests.clear();

        const nlohmann::json lJson = nlohmann::json::parse(InText.begin(), InText.end(), nullptr,
                                                           /*allow_exceptions*/ false);
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

        nlohmann::json lExpanded = nlohmann::json::array();
        if (!ExpandItems(*lList, lExpanded, OutError))
        {
            return false;
        }
        lList = &lExpanded;

        for (Uint64 lIndex = 0; lIndex < lList->size(); ++lIndex)
        {
            AutomationRequest lRequest;
            OpaaxString       lError;
            if (!ParseAutomationRequest((*lList)[lIndex], lRequest, lError))
            {
                OutError = OpaaxString::Format("request {}: {}", lIndex, lError);
                OutRequests.clear();
                return false;
            }

            if (lRequest.Id.IsEmpty())
            {
                lRequest.Id = OpaaxString::FromUInt(lIndex);
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
