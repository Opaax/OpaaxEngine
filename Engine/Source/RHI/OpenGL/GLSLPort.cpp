#include "RHI/OpenGL/GLSLPort.h"

#include "Core/String/CString.h"
#include "Core/String/Char.h"

namespace Opaax::GLSLPort
{
    namespace
    {
        bool IsIdentChar(const char InChar) noexcept
        {
            return Char::IsAlnum(InChar) || InChar == '_';
        }

        void SkipSpaces(const OpaaxStringView InText, Uint32& InOutAt) noexcept
        {
            while (InOutAt < InText.GetLength() && Char::IsSpace(InText[InOutAt]))
            {
                ++InOutAt;
            }
        }

        OpaaxStringView ReadIdentifier(const OpaaxStringView InText, Uint32& InOutAt) noexcept
        {
            const Uint32 lStart = InOutAt;
            while (InOutAt < InText.GetLength() && IsIdentChar(InText[InOutAt]))
            {
                ++InOutAt;
            }
            return InText.SubString(lStart, InOutAt - lStart);
        }

        /** "binding = 3" gives true and 3. Anything else (location, std140, "bindings") gives false. */
        bool ParseBinding(const OpaaxStringView InQualifier, Int32& OutValue)
        {
            constexpr Uint32 BINDING_LENGTH = 7;

            if (!InQualifier.StartsWith("binding"))
            {
                return false;
            }

            Uint32 lAt = BINDING_LENGTH;
            SkipSpaces(InQualifier, lAt);
            if (lAt >= InQualifier.GetLength() || InQualifier[lAt] != '=')
            {
                return false;
            }

            ++lAt;
            SkipSpaces(InQualifier, lAt);

            // The number is read from a terminated copy: the view goes on into the rest of the source.
            const OpaaxString lDigits = InQualifier.SubString(lAt).ToString();
            OutValue = static_cast<Int32>(CString::ToInt64(lDigits.CStr()));
            return true;
        }

        /**
         * What a layout qualifier at InAt (just after its closing parenthesis) applies to:
         * `uniform Block` or `uniform sampler2D Name[N]`. False for anything else.
         */
        bool ReadUniformTarget(const OpaaxString& InText, Uint32 InAt, ResourceBinding& OutBinding)
        {
            const OpaaxStringView lText = InText.View();

            SkipSpaces(lText, InAt);
            if (ReadIdentifier(lText, InAt) != "uniform")
            {
                return false;
            }

            SkipSpaces(lText, InAt);
            const OpaaxStringView lType = ReadIdentifier(lText, InAt);
            if (lType.IsEmpty())
            {
                return false;
            }

            // sampler2D, isampler2D, usampler2D, sampler2DArray, ...: an opaque uniform.
            if (lType.Contains("sampler"))
            {
                SkipSpaces(lText, InAt);
                OutBinding.Name   = ReadIdentifier(lText, InAt).ToString();
                OutBinding.bBlock = false;

                SkipSpaces(lText, InAt);
                if (InAt < lText.GetLength() && lText[InAt] == '[')
                {
                    OutBinding.ArraySize = static_cast<Int32>(CString::ToInt64(InText.CStr() + InAt + 1));
                }
            }
            else
            {
                OutBinding.Name   = lType.ToString();
                OutBinding.bBlock = true;
            }

            return !OutBinding.Name.IsEmpty() && OutBinding.ArraySize > 0;
        }
    }

    OpaaxString To410(const OpaaxString& InSource, TDynArray<ResourceBinding>& OutBindings)
    {
        OutBindings.clear();

        const OpaaxStringView lSource = InSource.View();

        OpaaxString lOut;
        lOut.Reserve(lSource.GetLength());

        Uint32 lAt = 0;
        while (lAt < lSource.GetLength())
        {
            const bool bLineStart = (lAt == 0 || lSource[lAt - 1] == '\n');

            // The version line.
            if (bLineStart && lSource.SubString(lAt).StartsWith("#version"))
            {
                const Int32 lLineEnd = lSource.Find('\n', lAt);
                lOut += "#version 410 core";
                lAt = (lLineEnd < 0) ? lSource.GetLength() : static_cast<Uint32>(lLineEnd);
                continue;
            }

            // A layout qualifier, as a whole word.
            const bool bLayout = lSource.SubString(lAt).StartsWith("layout")
                              && (lAt == 0 || !IsIdentChar(lSource[lAt - 1]))
                              && (lAt + 6 < lSource.GetLength() && !IsIdentChar(lSource[lAt + 6]));
            if (bLayout)
            {
                Uint32 lOpen = lAt + 6;
                SkipSpaces(lSource, lOpen);
                const Int32 lClose = (lOpen < lSource.GetLength() && lSource[lOpen] == '(') ? lSource.Find(')', lOpen) : -1;

                if (lClose >= 0)
                {
                    // The qualifiers, without the binding.
                    TDynArray<OpaaxStringView> lKept;
                    Int32 lBinding = -1;

                    const OpaaxStringView lInside = lSource.SubString(lOpen + 1, static_cast<Uint32>(lClose) - lOpen - 1);
                    Uint32 lStart = 0;
                    while (lStart <= lInside.GetLength())
                    {
                        const Int32           lComma     = lInside.Find(',', lStart);
                        const OpaaxStringView lQualifier = (lComma < 0 ? lInside.SubString(lStart)
                                                                       : lInside.SubString(lStart, static_cast<Uint32>(lComma) - lStart)).Trim();

                        Int32 lValue = 0;
                        if (ParseBinding(lQualifier, lValue)) { lBinding = lValue; }
                        else if (!lQualifier.IsEmpty())       { lKept.push_back(lQualifier); }

                        if (lComma < 0) { break; }
                        lStart = static_cast<Uint32>(lComma) + 1;
                    }

                    ResourceBinding lTarget;
                    lTarget.Binding = lBinding;

                    if (lBinding >= 0 && ReadUniformTarget(InSource, static_cast<Uint32>(lClose) + 1, lTarget))
                    {
                        OutBindings.push_back(lTarget);

                        if (!lKept.empty())
                        {
                            lOut += "layout(";
                            for (Uint64 lIndex = 0; lIndex < lKept.size(); ++lIndex)
                            {
                                if (lIndex > 0) { lOut += ", "; }
                                lOut += lKept[lIndex];
                            }
                            lOut += ")";
                        }

                        lAt = static_cast<Uint32>(lClose) + 1;
                        continue;
                    }
                }
            }

            lOut += lSource[lAt];
            ++lAt;
        }

        return lOut;
    }
}
