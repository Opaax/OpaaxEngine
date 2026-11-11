#include "DevConsole/DevConsoleCompletion.h"

#include "Core/Maths/Maths.h"
#include "Core/String/Char.h"

namespace Opaax
{
    namespace
    {
        /** The words of InText, split on spaces (the words before the one being completed). */
        TDynArray<OpaaxString> Words(const OpaaxString& InText)
        {
            TDynArray<OpaaxString> lWords;
            Uint32                 lStart = 0;
            for (Uint32 lAt = 0; lAt <= InText.GetLength(); ++lAt)
            {
                if (lAt == InText.GetLength() || Char::IsSpace(InText[lAt]))
                {
                    if (lAt > lStart)
                    {
                        lWords.push_back(InText.SubString(lStart, lAt - lStart));
                    }
                    lStart = lAt + 1;
                }
            }
            return lWords;
        }

        /** The longest start every one of InMatches shares, ignoring case, in the first one's case. */
        OpaaxString CommonStart(const TDynArray<OpaaxString>& InMatches)
        {
            const OpaaxString& lFirst  = InMatches.front();
            Uint32             lLength = lFirst.GetLength();
            for (const OpaaxString& lOther : InMatches)
            {
                Uint32 lSame = 0;
                const Uint32 lMost = Maths::Min(lLength, lOther.GetLength());
                while (lSame < lMost
                       && OpaaxString::ToLowerChar(lFirst[lSame]) == OpaaxString::ToLowerChar(lOther[lSame]))
                {
                    ++lSame;
                }
                lLength = lSame;
            }
            return lFirst.SubString(0, lLength);
        }

        /** The parameter the next word fills: InNamed and the plain words already typed skip theirs. */
        const CommandParam* NextParam(const CommandSpec& InSpec, const TDynArray<OpaaxString>& InArgs)
        {
            const TDynArray<CommandParam>& lParams = InSpec.GetParams();
            TDynArray<bool>                lGiven(lParams.size(), false);
            Uint32                         lPlain = 0;
            for (const OpaaxString& lWord : InArgs)
            {
                const Int32 lEquals = lWord.Find('=');
                if (lEquals <= 0)
                {
                    ++lPlain;
                    continue;
                }
                const OpaaxString lKey = lWord.SubString(0, static_cast<Uint32>(lEquals));
                for (Uint64 lIndex = 0; lIndex < lParams.size(); ++lIndex)
                {
                    lGiven[lIndex] = lGiven[lIndex] || lParams[lIndex].Name == lKey;
                }
            }

            for (Uint64 lIndex = 0; lIndex < lParams.size(); ++lIndex)
            {
                if (lGiven[lIndex])
                {
                    continue;
                }
                if (lPlain == 0)
                {
                    return &lParams[lIndex];
                }
                --lPlain;
            }
            return nullptr;
        }
    }

    DevConsoleCompletion CompleteCommandText(const OpaaxString& InLine, const TDynArray<CommandSpec>& InSpecs)
    {
        DevConsoleCompletion lResult;
        lResult.Line = InLine;

        // A word inside a quote or a bracket is not completed.
        if (InLine.Find('"') >= 0 || InLine.Find('{') >= 0 || InLine.Find('[') >= 0)
        {
            return lResult;
        }

        // The word being completed starts after the last space.
        Uint32 lWordStart = InLine.GetLength();
        while (lWordStart > 0 && !Char::IsSpace(InLine[lWordStart - 1]))
        {
            --lWordStart;
        }
        const OpaaxString            lHead   = InLine.SubString(0, lWordStart);
        OpaaxString                  lWord   = InLine.SubString(lWordStart);
        const TDynArray<OpaaxString> lBefore = Words(lHead);

        // What the word is matched against, and where in it the match goes (after key=).
        TDynArray<OpaaxString> lCandidates;
        Uint32                 lValueStart = 0;
        if (lBefore.empty())
        {
            for (const CommandSpec& lSpec : InSpecs)
            {
                lCandidates.push_back(lSpec.GetName());
            }
        }
        else
        {
            const CommandSpec* lSpec = nullptr;
            for (const CommandSpec& lCandidate : InSpecs)
            {
                lSpec = lCandidate.GetName() == lBefore.front() ? &lCandidate : lSpec;
            }
            if (lSpec == nullptr)
            {
                return lResult;
            }

            const TDynArray<OpaaxString> lArgs(lBefore.begin() + 1, lBefore.end());
            const CommandParam*          lParam  = nullptr;
            const Int32                  lEquals = lWord.Find('=');
            if (lEquals > 0)
            {
                const OpaaxString lKey = lWord.SubString(0, static_cast<Uint32>(lEquals));
                for (const CommandParam& lDeclared : lSpec->GetParams())
                {
                    lParam = lDeclared.Name == lKey ? &lDeclared : lParam;
                }
                lValueStart = static_cast<Uint32>(lEquals) + 1;
            }
            else
            {
                lParam = NextParam(*lSpec, lArgs);
            }

            if (lParam == nullptr)
            {
                return lResult;
            }
            lCandidates = lParam->Choices;
            if (lParam->bCommandName)
            {
                for (const CommandSpec& lNamed : InSpecs)
                {
                    lCandidates.push_back(lNamed.GetName());
                }
            }
        }

        const OpaaxString lTyped = lWord.SubString(lValueStart).ToLower();
        TDynArray<OpaaxString> lMatches;
        for (const OpaaxString& lCandidate : lCandidates)
        {
            if (lCandidate.ToLower().StartsWith(lTyped))
            {
                lMatches.push_back(lCandidate);
            }
        }
        if (lMatches.empty())
        {
            return lResult;
        }

        const OpaaxString lKeyPart = lWord.SubString(0, lValueStart);
        if (lMatches.size() == 1)
        {
            lResult.Line = lHead + lKeyPart + lMatches.front() + " ";
            return lResult;
        }

        // Several: as far as they agree, never shorter than what was typed.
        const OpaaxString lCommon = CommonStart(lMatches);
        if (lCommon.GetLength() >= lTyped.GetLength())
        {
            lResult.Line = lHead + lKeyPart + lCommon;
        }
        lResult.Matches = Move(lMatches);
        return lResult;
    }
}
