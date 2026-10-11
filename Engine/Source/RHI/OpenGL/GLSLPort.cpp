#include "RHI/OpenGL/GLSLPort.h"

#include <cctype>
#include <cstdlib>

namespace Opaax::GLSLPort
{
    namespace
    {
        bool IsIdentChar(const char InChar) noexcept
        {
            return std::isalnum(static_cast<unsigned char>(InChar)) != 0 || InChar == '_';
        }

        void SkipSpaces(const std::string& InText, size_t& InOutAt) noexcept
        {
            while (InOutAt < InText.size() && std::isspace(static_cast<unsigned char>(InText[InOutAt])) != 0)
            {
                ++InOutAt;
            }
        }

        std::string ReadIdentifier(const std::string& InText, size_t& InOutAt)
        {
            const size_t lStart = InOutAt;
            while (InOutAt < InText.size() && IsIdentChar(InText[InOutAt]))
            {
                ++InOutAt;
            }
            return InText.substr(lStart, InOutAt - lStart);
        }

        std::string Trim(const std::string& InText)
        {
            size_t lBegin = 0;
            size_t lEnd   = InText.size();
            while (lBegin < lEnd && std::isspace(static_cast<unsigned char>(InText[lBegin])) != 0) { ++lBegin; }
            while (lEnd > lBegin && std::isspace(static_cast<unsigned char>(InText[lEnd - 1])) != 0) { --lEnd; }
            return InText.substr(lBegin, lEnd - lBegin);
        }

        /** "binding = 3" gives true and 3. Anything else (location, std140, "bindings") gives false. */
        bool ParseBinding(const std::string& InQualifier, Int32& OutValue)
        {
            constexpr size_t BINDING_LENGTH = 7;

            if (InQualifier.compare(0, BINDING_LENGTH, "binding") != 0)
            {
                return false;
            }

            size_t lAt = BINDING_LENGTH;
            SkipSpaces(InQualifier, lAt);
            if (lAt >= InQualifier.size() || InQualifier[lAt] != '=')
            {
                return false;
            }

            ++lAt;
            SkipSpaces(InQualifier, lAt);
            OutValue = static_cast<Int32>(std::strtol(InQualifier.c_str() + lAt, nullptr, 10));
            return true;
        }

        /**
         * What a layout qualifier at InAt (just after its closing parenthesis) applies to:
         * `uniform Block` or `uniform sampler2D Name[N]`. False for anything else.
         */
        bool ReadUniformTarget(const std::string& InText, size_t InAt, ResourceBinding& OutBinding)
        {
            SkipSpaces(InText, InAt);
            if (ReadIdentifier(InText, InAt) != "uniform")
            {
                return false;
            }

            SkipSpaces(InText, InAt);
            const std::string lType = ReadIdentifier(InText, InAt);
            if (lType.empty())
            {
                return false;
            }

            // sampler2D, isampler2D, usampler2D, sampler2DArray, ...: an opaque uniform.
            if (lType.find("sampler") != std::string::npos)
            {
                SkipSpaces(InText, InAt);
                OutBinding.Name   = ReadIdentifier(InText, InAt);
                OutBinding.bBlock = false;

                SkipSpaces(InText, InAt);
                if (InAt < InText.size() && InText[InAt] == '[')
                {
                    OutBinding.ArraySize = static_cast<Int32>(std::strtol(InText.c_str() + InAt + 1, nullptr, 10));
                }
            }
            else
            {
                OutBinding.Name   = lType;
                OutBinding.bBlock = true;
            }

            return !OutBinding.Name.empty() && OutBinding.ArraySize > 0;
        }
    }

    std::string To410(const std::string& InSource, TDynArray<ResourceBinding>& OutBindings)
    {
        OutBindings.clear();

        std::string lOut;
        lOut.reserve(InSource.size());

        size_t lAt = 0;
        while (lAt < InSource.size())
        {
            const bool bLineStart = (lAt == 0 || InSource[lAt - 1] == '\n');

            // The version line.
            if (bLineStart && InSource.compare(lAt, 8, "#version") == 0)
            {
                const size_t lLineEnd = InSource.find('\n', lAt);
                lOut += "#version 410 core";
                lAt = (lLineEnd == std::string::npos) ? InSource.size() : lLineEnd;
                continue;
            }

            // A layout qualifier, as a whole word.
            const bool bLayout = InSource.compare(lAt, 6, "layout") == 0
                              && (lAt == 0 || !IsIdentChar(InSource[lAt - 1]))
                              && (lAt + 6 < InSource.size() && !IsIdentChar(InSource[lAt + 6]));
            if (bLayout)
            {
                size_t lOpen = lAt + 6;
                SkipSpaces(InSource, lOpen);
                const size_t lClose = (lOpen < InSource.size() && InSource[lOpen] == '(') ? InSource.find(')', lOpen) : std::string::npos;

                if (lClose != std::string::npos)
                {
                    // The qualifiers, without the binding.
                    TDynArray<std::string> lKept;
                    Int32 lBinding = -1;

                    const std::string lInside = InSource.substr(lOpen + 1, lClose - lOpen - 1);
                    size_t lStart = 0;
                    while (lStart <= lInside.size())
                    {
                        const size_t lComma = lInside.find(',', lStart);
                        const std::string lQualifier = Trim(lInside.substr(lStart, lComma == std::string::npos ? std::string::npos : lComma - lStart));

                        Int32 lValue = 0;
                        if (ParseBinding(lQualifier, lValue)) { lBinding = lValue; }
                        else if (!lQualifier.empty())         { lKept.push_back(lQualifier); }

                        if (lComma == std::string::npos) { break; }
                        lStart = lComma + 1;
                    }

                    ResourceBinding lTarget;
                    lTarget.Binding = lBinding;

                    if (lBinding >= 0 && ReadUniformTarget(InSource, lClose + 1, lTarget))
                    {
                        OutBindings.push_back(lTarget);

                        if (!lKept.empty())
                        {
                            lOut += "layout(";
                            for (size_t lIndex = 0; lIndex < lKept.size(); ++lIndex)
                            {
                                if (lIndex > 0) { lOut += ", "; }
                                lOut += lKept[lIndex];
                            }
                            lOut += ")";
                        }

                        lAt = lClose + 1;
                        continue;
                    }
                }
            }

            lOut += InSource[lAt];
            ++lAt;
        }

        return lOut;
    }
}
