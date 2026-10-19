#include "Renderer/ShaderSource.h"

#include "Core/Log/Logger.h"

namespace Opaax
{
    inline constexpr LogCategory LogShaderSource{"ShaderSource"};

    ShaderDesc ShaderSource::ParseShaderStages(const OpaaxString& InSource, const OpaaxString& InDebugName)
    {
        ShaderDesc lDesc;
        lDesc.DebugName = InDebugName;

        const OpaaxStringView lSource(InSource);
        OpaaxString           lVert;
        OpaaxString           lFrag;
        int                   lStage = -1; // 0 = vertex, 1 = fragment, -1 = none

        // Line by line; a last line without a line break counts, an empty one after it does not.
        for (Uint32 lLineStart = 0; lLineStart < lSource.GetLength();)
        {
            const Int32  lBreak   = lSource.Find('\n', lLineStart);
            const Uint32 lLineEnd = (lBreak < 0) ? lSource.GetLength() : static_cast<Uint32>(lBreak);

            OpaaxStringView lLine = lSource.SubString(lLineStart, lLineEnd - lLineStart);
            lLineStart = lLineEnd + 1;

            if (lLine.EndsWith("\r")) { lLine.RemoveSuffix(1); } // CRLF

            Uint32 lFirst = 0;
            while (lFirst < lLine.GetLength() && (lLine[lFirst] == ' ' || lLine[lFirst] == '\t')) { ++lFirst; }

            if (lLine.SubString(lFirst).StartsWith("#type"))
            {
                const OpaaxStringView lRest = lLine.SubString(lFirst + 5);
                if      (lRest.Contains("vertex"))   { lStage = 0; }
                else if (lRest.Contains("fragment") ||
                         lRest.Contains("pixel"))    { lStage = 1; }
                else                                 { lStage = -1; }
                continue;
            }

            if      (lStage == 0) { lVert += lLine; lVert += '\n'; }
            else if (lStage == 1) { lFrag += lLine; lFrag += '\n'; }
        }

        lDesc.VertexSrc   = Move(lVert);
        lDesc.FragmentSrc = Move(lFrag);
        return lDesc;
    }

    ShaderDesc ShaderSource::FromSource(const OpaaxString& InSource, const OpaaxString& InDebugName)
    {
        ShaderDesc lDesc = ParseShaderStages(InSource, InDebugName);
        if (lDesc.VertexSrc.IsEmpty() || lDesc.FragmentSrc.IsEmpty())
        {
            OPAAX_LOG(LogShaderSource, Error, "'{}' missing a vertex or fragment '#type' section", InDebugName.CStr());
            return ShaderDesc{};
        }

        return lDesc;
    }
}
