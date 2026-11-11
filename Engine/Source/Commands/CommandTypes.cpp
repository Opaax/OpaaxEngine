#include "Commands/CommandTypes.h"

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax
{
    // =========================================================================
    // CommandResult
    // =========================================================================
    CommandResult CommandResult::Ok(nlohmann::json InValue)
    {
        CommandResult lResult;
        lResult.Value = Move(InValue);
        return lResult;
    }

    CommandResult CommandResult::Fail(OpaaxString InError)
    {
        CommandResult lResult;
        lResult.bOk   = false;
        lResult.Error = Move(InError);
        return lResult;
    }

    // =========================================================================
    // Parameters
    // =========================================================================
    const char* ToString(ECommandParamType InType) noexcept
    {
        switch (InType)
        {
        case ECommandParamType::Bool:   return "bool";
        case ECommandParamType::Int:    return "int";
        case ECommandParamType::Float:  return "float";
        case ECommandParamType::String: return "string";
        case ECommandParamType::Any:    break;
        }
        return "any";
    }

    // =========================================================================
    // CommandSpec
    // =========================================================================
    CommandSpec::CommandSpec(OpaaxString InName, OpaaxString InHelp)
        : m_Name(Move(InName))
        , m_Help(Move(InHelp))
    {
    }

    CommandSpec& CommandSpec::Param(OpaaxString InName, ECommandParamType InType, OpaaxString InHelp)
    {
        CommandParam lParam;
        lParam.Name = Move(InName);
        lParam.Type = InType;
        lParam.Help = Move(InHelp);
        m_Params.push_back(Move(lParam));
        return *this;
    }

    CommandSpec& CommandSpec::Choices(TDynArray<OpaaxString> InChoices)
    {
        if (m_Params.empty())
        {
            OPAAX_LOG(LogCommands, Error, "Command '{}': Choices before any Param", m_Name);
            return *this;
        }
        m_Params.back().Choices = Move(InChoices);
        return *this;
    }

    CommandSpec& CommandSpec::Optional()
    {
        if (m_Params.empty())
        {
            OPAAX_LOG(LogCommands, Error, "Command '{}': Optional before any Param", m_Name);
            return *this;
        }
        m_Params.back().bOptional = true;
        return *this;
    }

    CommandSpec& CommandSpec::Cheat()
    {
        m_bCheat = true;
        return *this;
    }

    CommandSpec& CommandSpec::DevOnly()
    {
        m_bDevOnly = true;
        return *this;
    }

    nlohmann::json CommandSpec::ToJson() const
    {
        nlohmann::json lJson{ { "name", m_Name }, { "help", m_Help } };
        if (m_bCheat)
        {
            lJson["cheat"] = true;
        }
        if (m_bDevOnly)
        {
            lJson["devOnly"] = true;
        }
        if (m_Params.empty())
        {
            return lJson;
        }

        nlohmann::json lParams = nlohmann::json::array();
        for (const CommandParam& lParam : m_Params)
        {
            nlohmann::json lItem{ { "name", lParam.Name }, { "type", ToString(lParam.Type) }, { "help", lParam.Help } };
            if (lParam.bOptional)
            {
                lItem["optional"] = true;
            }
            if (!lParam.Choices.empty())
            {
                lItem["choices"] = lParam.Choices;
            }
            lParams.push_back(Move(lItem));
        }
        lJson["params"] = Move(lParams);
        return lJson;
    }
}
