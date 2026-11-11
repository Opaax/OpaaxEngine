#include "Commands/CommandText.h"

#include "Application/Services/ICommandRegistry.h"
#include "Core/Maths/Maths.h"
#include "Core/String/Char.h"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax
{
    namespace
    {
        /** InText as JSON, or a discarded value when it is not JSON. */
        nlohmann::json ReadJson(const OpaaxString& InText)
        {
            return nlohmann::json::parse(InText.begin(), InText.end(), nullptr, /*allow_exceptions*/ false);
        }

        /** The length of the key in "key=value" at the start of InWord, or 0 when it has none. */
        Uint32 KeyLength(const OpaaxString& InWord)
        {
            Uint32 lAt = 0;
            while (lAt < InWord.GetLength() && (Char::IsAlnum(InWord[lAt]) || InWord[lAt] == '_'))
            {
                ++lAt;
            }

            const bool bStartsAsName = lAt > 0 && !Char::IsDigit(InWord[0u]);
            return (bStartsAsName && lAt < InWord.GetLength() && InWord[lAt] == '=') ? lAt : 0u;
        }

        /** A value as it reads: quoted text, or text unquoted. False, with OutError, for a bad quote. */
        bool ReadWord(const OpaaxString& InValue, CommandWord& OutWord, OpaaxString& OutError)
        {
            const bool bQuoted = InValue.GetLength() >= 2 && InValue[0u] == '"' && InValue[InValue.GetLength() - 1] == '"';
            if (!bQuoted)
            {
                OutWord.Text = InValue;
                return true;
            }

            const nlohmann::json lText = ReadJson(InValue);
            if (!lText.is_string())
            {
                OutError = "cannot read the quoted text " + InValue + ": a quote inside it is written \\\"";
                return false;
            }
            OutWord.Text    = lText.get<OpaaxString>();
            OutWord.bQuoted = true;
            return true;
        }

        /** The text a choice is compared with: a string as it is, any other value as JSON. */
        OpaaxString ChoiceText(const nlohmann::json& InValue)
        {
            return InValue.is_string() ? InValue.get<OpaaxString>() : ToJsonText(InValue);
        }

        /** What a word gives InParam, or false with OutError. */
        bool ReadValue(const CommandSpec& InSpec, const CommandParam& InParam, const CommandWord& InWord,
                       nlohmann::json& OutValue, OpaaxString& OutError)
        {
            // Quoted, or for a text parameter: the text itself. Else what it reads as, or text.
            if (InWord.bQuoted || InParam.Type == ECommandParamType::String)
            {
                OutValue = nlohmann::json(InWord.Text);
            }
            else
            {
                const nlohmann::json lJson = ReadJson(InWord.Text);
                OutValue = lJson.is_discarded() ? nlohmann::json(InWord.Text) : lJson;
            }

            const auto lWrongType = [&](const char* InExpected)
            {
                OutError = InSpec.GetName() + ": " + InParam.Name + " is " + InExpected + ", not " + InWord.Text
                         + " (usage: " + FormatCommandUsage(InSpec) + ")";
                return false;
            };

            switch (InParam.Type)
            {
            case ECommandParamType::Bool:
                if (!OutValue.is_boolean()) { return lWrongType("true or false"); }
                break;
            case ECommandParamType::Int:
                if (!OutValue.is_number_integer()) { return lWrongType("a whole number"); }
                break;
            case ECommandParamType::Float:
                if (!OutValue.is_number()) { return lWrongType("a number"); }
                break;
            case ECommandParamType::String:
            case ECommandParamType::Any:
                break;
            }

            // Choices are closed for a typed parameter, hints for Any.
            if (!InParam.Choices.empty() && InParam.Type != ECommandParamType::Any)
            {
                const OpaaxString lText = ChoiceText(OutValue);
                bool bChosen = false;
                OpaaxString lList;
                for (const OpaaxString& lChoice : InParam.Choices)
                {
                    bChosen = bChosen || lChoice == lText;
                    lList.Append(lList.IsEmpty() ? "" : ", ");
                    lList.Append(lChoice);
                }
                if (!bChosen)
                {
                    OutError = InSpec.GetName() + ": " + InParam.Name + " is one of " + lList + ", not " + lText;
                    return false;
                }
            }
            return true;
        }

        /** The names of InSpec's parameters, "a, b", or "nothing". */
        OpaaxString ParamNames(const CommandSpec& InSpec)
        {
            OpaaxString lNames;
            for (const CommandParam& lParam : InSpec.GetParams())
            {
                lNames.Append(lNames.IsEmpty() ? "" : ", ");
                lNames.Append(lParam.Name);
            }
            return lNames.IsEmpty() ? OpaaxString("nothing") : lNames;
        }

        CommandTextResult Fail(OpaaxString InError)
        {
            CommandTextResult lResult;
            lResult.Error = Move(InError);
            return lResult;
        }
    }

    // =========================================================================
    // Words
    // =========================================================================
    bool SplitCommandText(const OpaaxString& InLine, TDynArray<CommandWord>& OutWords, OpaaxString& OutError)
    {
        OutWords.clear();

        const Uint32 lLength = InLine.GetLength();
        Uint32       lAt     = 0;
        while (lAt < lLength)
        {
            while (lAt < lLength && Char::IsSpace(InLine[lAt]))
            {
                ++lAt;
            }
            if (lAt >= lLength)
            {
                break;
            }

            // A word ends at a space outside quotes and brackets.
            const Uint32 lStart    = lAt;
            bool         bInString = false;
            Int32        lDepth    = 0;
            for (; lAt < lLength; ++lAt)
            {
                const char lChar = InLine[lAt];
                if (bInString)
                {
                    if (lChar == '\\')
                    {
                        ++lAt;   // the escaped character
                    }
                    else if (lChar == '"')
                    {
                        bInString = false;
                    }
                    continue;
                }

                if (lChar == '"')                      { bInString = true; }
                else if (lChar == '{' || lChar == '[') { ++lDepth; }
                else if (lChar == '}' || lChar == ']') { --lDepth; }
                else if (Char::IsSpace(lChar) && lDepth <= 0)
                {
                    break;
                }
            }

            const OpaaxString lRaw = InLine.SubString(lStart, Maths::Min(lAt, lLength) - lStart);
            if (bInString)
            {
                OutError = "a quote is not closed: " + lRaw;
                return false;
            }
            if (lDepth != 0)
            {
                OutError = (lDepth > 0 ? "a { or [ is not closed: " : "a } or ] closes nothing: ") + lRaw;
                return false;
            }

            CommandWord  lWord;
            const Uint32 lKey = KeyLength(lRaw);
            if (lKey > 0)
            {
                lWord.Key = lRaw.SubString(0, lKey);
            }
            if (!ReadWord(lKey > 0 ? lRaw.SubString(lKey + 1) : lRaw, lWord, OutError))
            {
                return false;
            }
            OutWords.push_back(Move(lWord));
        }
        return true;
    }

    // =========================================================================
    // Params
    // =========================================================================
    CommandTextResult BuildCommandParams(const CommandSpec& InSpec, const TDynArray<CommandWord>& InArgs)
    {
        CommandTextResult lResult;
        lResult.Command = InSpec.GetName();

        // A lone JSON object is the whole params, as a script sends them.
        if (InArgs.size() == 1 && InArgs[0].Key.IsEmpty() && !InArgs[0].bQuoted && InArgs[0].Text.StartsWith("{"))
        {
            const nlohmann::json lObject = ReadJson(InArgs[0].Text);
            if (!lObject.is_object())
            {
                return Fail(InSpec.GetName() + ": cannot read " + InArgs[0].Text + " as a JSON object");
            }
            lResult.Params = lObject;
            lResult.bOk    = true;
            return lResult;
        }

        const TDynArray<CommandParam>& lDeclared = InSpec.GetParams();
        TDynArray<bool>                lGiven(lDeclared.size(), false);

        // Named values first, then the plain words fill what is left, in order.
        for (const CommandWord& lWord : InArgs)
        {
            if (lWord.Key.IsEmpty())
            {
                continue;
            }

            if (lDeclared.empty())
            {
                const nlohmann::json lJson = lWord.bQuoted ? nlohmann::json(lWord.Text) : ReadJson(lWord.Text);
                lResult.Params[lWord.Key.CStr()] = lJson.is_discarded() ? nlohmann::json(lWord.Text) : lJson;
                continue;
            }

            Uint64 lIndex = 0;
            while (lIndex < lDeclared.size() && lDeclared[lIndex].Name != lWord.Key)
            {
                ++lIndex;
            }
            if (lIndex == lDeclared.size())
            {
                return Fail(InSpec.GetName() + " has no parameter " + lWord.Key + ": it takes " + ParamNames(InSpec));
            }
            if (lGiven[lIndex])
            {
                return Fail(InSpec.GetName() + ": " + lWord.Key + " is given twice");
            }

            nlohmann::json lValue;
            if (!ReadValue(InSpec, lDeclared[lIndex], lWord, lValue, lResult.Error))
            {
                return lResult;
            }
            lResult.Params[lWord.Key.CStr()] = Move(lValue);
            lGiven[lIndex] = true;
        }

        Uint64 lNext = 0;
        for (const CommandWord& lWord : InArgs)
        {
            if (!lWord.Key.IsEmpty())
            {
                continue;
            }

            while (lNext < lDeclared.size() && lGiven[lNext])
            {
                ++lNext;
            }
            if (lNext >= lDeclared.size())
            {
                return Fail(lDeclared.empty()
                    ? InSpec.GetName() + " declares no parameters: give them as name=value, or as a JSON object"
                    : InSpec.GetName() + " takes " + ParamNames(InSpec) + ", and " + lWord.Text + " is one too many "
                      "(usage: " + FormatCommandUsage(InSpec) + ")");
            }

            nlohmann::json lValue;
            if (!ReadValue(InSpec, lDeclared[lNext], lWord, lValue, lResult.Error))
            {
                return lResult;
            }
            lResult.Params[lDeclared[lNext].Name.CStr()] = Move(lValue);
            lGiven[lNext] = true;
        }

        for (Uint64 lIndex = 0; lIndex < lDeclared.size(); ++lIndex)
        {
            if (!lGiven[lIndex] && !lDeclared[lIndex].bOptional)
            {
                return Fail(InSpec.GetName() + " needs " + lDeclared[lIndex].Name + ", " + lDeclared[lIndex].Help
                            + " (usage: " + FormatCommandUsage(InSpec) + ")");
            }
        }

        lResult.bOk = true;
        return lResult;
    }

    CommandTextResult ParseCommandText(const OpaaxString& InLine, const ICommandRegistry& InCommands)
    {
        TDynArray<CommandWord> lWords;
        OpaaxString            lError;
        if (!SplitCommandText(InLine, lWords, lError))
        {
            return Fail(lError);
        }
        if (lWords.empty())
        {
            return Fail("type a command (commands.list lists them)");
        }
        if (!lWords[0].Key.IsEmpty() || lWords[0].bQuoted)
        {
            return Fail("a line starts with a command's name (commands.list lists them)");
        }

        const TSharedPtr<const RegisteredCommand> lCommand = InCommands.Find(lWords[0].Text);
        if (lCommand == nullptr)
        {
            return Fail("unknown command '" + lWords[0].Text + "' (commands.list lists them)");
        }

        lWords.erase(lWords.begin());
        return BuildCommandParams(lCommand->Spec, lWords);
    }

    // =========================================================================
    // Usage
    // =========================================================================
    OpaaxString FormatCommandUsage(const CommandSpec& InSpec)
    {
        OpaaxString lUsage = InSpec.GetName();
        for (const CommandParam& lParam : InSpec.GetParams())
        {
            lUsage.Append(lParam.bOptional ? " [" : " <");
            lUsage.Append(lParam.Name);
            if (lParam.Type != ECommandParamType::Any)
            {
                lUsage.Append(':');
                lUsage.Append(ToString(lParam.Type));
            }
            lUsage.Append(lParam.bOptional ? "]" : ">");
        }
        return lUsage;
    }
}
