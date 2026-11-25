#include "Renderer/Materials/ShaderParams.h"

#include "Core/String/CString.h"
#include "Core/String/Char.h"

namespace Opaax
{
    namespace
    {
        constexpr const char* k_SlotNames[] = {
            "Param0", "Param1", "Param2", "Param3", "Param4", "Param5", "Param6", "Param7",
            "Color0", "Color1",
            "Texture0", "Texture1",
        };
        static_assert(sizeof(k_SlotNames) / sizeof(k_SlotNames[0]) == static_cast<size_t>(EShaderParamSlot::Count),
                      "a slot without a name");

        /** Takes the word at the start of InOutText (letters, digits, '_'); empty when there is none. */
        OpaaxStringView TakeWord(OpaaxStringView& InOutText)
        {
            Uint32 lLength = 0;
            while (lLength < InOutText.GetLength() && (Char::IsAlnum(InOutText[lLength]) || InOutText[lLength] == '_'))
            {
                ++lLength;
            }

            const OpaaxStringView lWord = InOutText.SubString(0, lLength);
            InOutText = InOutText.SubString(lLength).TrimStart();
            return lWord;
        }

        /** A GLSL name the shader can use: a letter or '_' first, not one of the engine's prefixes. */
        bool IsUsableName(const OpaaxStringView InName)
        {
            if (InName.IsEmpty() || Char::IsDigit(InName[0]))
            {
                return false;
            }
            return !InName.StartsWith("u_") && !InName.StartsWith("v_") && !InName.StartsWith("gl_");
        }

        /** Reads one number at the start of InOutText, then moves past it. */
        bool TakeNumber(OpaaxStringView& InOutText, float& OutValue)
        {
            const OpaaxString lText   = InOutText.ToString();
            const char*       lEnd    = nullptr;
            const double      lNumber = CString::ToDouble(lText.CStr(), &lEnd);
            const Uint32      lRead   = static_cast<Uint32>(lEnd - lText.CStr());
            if (lRead == 0)
            {
                return false;
            }

            OutValue  = static_cast<float>(lNumber);
            InOutText = InOutText.SubString(lRead).TrimStart();
            return true;
        }

        /** Reads "(n)" or "(n, m)" after an option's name. */
        bool TakeArguments(OpaaxStringView& InOutText, float* OutValues, const Uint32 InCount)
        {
            if (!InOutText.StartsWith("(")) { return false; }
            InOutText = InOutText.SubString(1).TrimStart();

            for (Uint32 i = 0; i < InCount; ++i)
            {
                if (i > 0)
                {
                    if (!InOutText.StartsWith(",")) { return false; }
                    InOutText = InOutText.SubString(1).TrimStart();
                }
                if (!TakeNumber(InOutText, OutValues[i])) { return false; }
            }

            if (!InOutText.StartsWith(")")) { return false; }
            InOutText = InOutText.SubString(1).TrimStart();
            return true;
        }

        /** Reads what follows `@param` on one line; false with OutWhy when the line is refused. */
        bool ParseDeclaration(OpaaxStringView InText, ShaderParam& OutParam, OpaaxString& OutWhy)
        {
            const OpaaxStringView lSlot = TakeWord(InText);
            if (!ShaderParams::SlotFromName(lSlot, OutParam.Slot))
            {
                OutWhy = OpaaxString::Format("unknown slot '{}' (Param0..7, Color0..1, Texture0..1)", lSlot);
                return false;
            }

            const OpaaxStringView lName = TakeWord(InText);
            if (!IsUsableName(lName))
            {
                OutWhy = OpaaxString::Format("'{}' is not a name the shader can use (letters, digits, '_'; "
                                             "not starting with a digit, u_, v_ or gl_)", lName);
                return false;
            }
            OutParam.Name = lName.ToString();

            if (InText.StartsWith("\""))
            {
                const Int32 lClose = InText.Find('"', 1);
                if (lClose < 0)
                {
                    OutWhy = "the tooltip has no closing '\"'";
                    return false;
                }
                OutParam.Tooltip = InText.SubString(1, static_cast<Uint32>(lClose) - 1).ToString();
                InText = InText.SubString(static_cast<Uint32>(lClose) + 1).TrimStart();
            }

            while (!InText.IsEmpty())
            {
                const OpaaxStringView lOption = TakeWord(InText);
                if (lOption == "range" || lOption == "default")
                {
                    if (!ShaderParams::IsNumber(OutParam.Slot))
                    {
                        OutWhy = OpaaxString::Format("{}() is for Param0..7, not {}", lOption,
                                                     ShaderParams::SlotName(OutParam.Slot));
                        return false;
                    }

                    if (lOption == "range")
                    {
                        float lRange[2] = { 0.f, 0.f };
                        if (!TakeArguments(InText, lRange, 2) || lRange[0] >= lRange[1])
                        {
                            OutWhy = "range() takes two numbers, the first below the second: range(0, 1)";
                            return false;
                        }
                        OutParam.bHasRange = true;
                        OutParam.RangeMin  = lRange[0];
                        OutParam.RangeMax  = lRange[1];
                    }
                    else
                    {
                        if (!TakeArguments(InText, &OutParam.Default, 1))
                        {
                            OutWhy = "default() takes one number: default(0.5)";
                            return false;
                        }
                        OutParam.bHasDefault = true;
                    }
                    continue;
                }

                OutWhy = OpaaxString::Format("unknown option '{}' (range, default)",
                                             lOption.IsEmpty() ? InText.SubString(0, 1) : lOption);
                return false;
            }

            return true;
        }
    }

    const ShaderParam* ShaderParamList::Find(const EShaderParamSlot InSlot) const noexcept
    {
        for (const ShaderParam& lParam : Params)
        {
            if (lParam.Slot == InSlot) { return &lParam; }
        }
        return nullptr;
    }

    ShaderParamList ShaderParams::Parse(const OpaaxStringView InSource)
    {
        ShaderParamList lList;

        Uint32 lLineNumber = 0;
        for (Uint32 lLineStart = 0; lLineStart < InSource.GetLength();)
        {
            const Int32  lBreak   = InSource.Find('\n', lLineStart);
            const Uint32 lLineEnd = (lBreak < 0) ? InSource.GetLength() : static_cast<Uint32>(lBreak);
            OpaaxStringView lLine = InSource.SubString(lLineStart, lLineEnd - lLineStart).Trim();
            lLineStart = lLineEnd + 1;
            ++lLineNumber;

            if (!lLine.StartsWith("//")) { continue; }
            lLine = lLine.SubString(2).TrimStart();
            if (!lLine.StartsWith("@param")) { continue; }
            lLine = lLine.SubString(6);
            if (!lLine.IsEmpty() && !Char::IsSpace(lLine[0])) { continue; }   // "@parameter" is prose

            ShaderParam lParam;
            OpaaxString lWhy;
            if (!ParseDeclaration(lLine.TrimStart(), lParam, lWhy))
            {
                lList.Errors.emplace_back(OpaaxString::Format("line {}: {}", lLineNumber, lWhy));
                continue;
            }

            if (lList.Find(lParam.Slot) != nullptr)
            {
                lList.Errors.emplace_back(OpaaxString::Format("line {}: {} is declared twice", lLineNumber,
                                                              SlotName(lParam.Slot)));
                continue;
            }

            bool bNameTaken = false;
            for (const ShaderParam& lKnown : lList.Params)
            {
                bNameTaken = bNameTaken || lKnown.Name == lParam.Name;
            }
            if (bNameTaken)
            {
                lList.Errors.emplace_back(OpaaxString::Format("line {}: the name '{}' is used twice", lLineNumber,
                                                              lParam.Name.CStr()));
                continue;
            }

            lList.Params.emplace_back(Move(lParam));
        }

        return lList;
    }

    const char* ShaderParams::SlotName(const EShaderParamSlot InSlot) noexcept
    {
        const Uint32 lIndex = static_cast<Uint32>(InSlot);
        return lIndex < static_cast<Uint32>(EShaderParamSlot::Count) ? k_SlotNames[lIndex] : "";
    }

    bool ShaderParams::SlotFromName(const OpaaxStringView InName, EShaderParamSlot& OutSlot) noexcept
    {
        for (Uint32 i = 0; i < static_cast<Uint32>(EShaderParamSlot::Count); ++i)
        {
            if (InName == k_SlotNames[i])
            {
                OutSlot = static_cast<EShaderParamSlot>(i);
                return true;
            }
        }
        return false;
    }

    bool ShaderParams::IsNumber(const EShaderParamSlot InSlot) noexcept
    {
        return static_cast<Uint32>(InSlot) <= static_cast<Uint32>(EShaderParamSlot::Param7);
    }

    OpaaxString ShaderParams::Expression(const EShaderParamSlot InSlot)
    {
        constexpr const char* k_Components[] = { "x", "y", "z", "w" };
        const Uint32 lIndex = static_cast<Uint32>(InSlot);

        if (IsNumber(InSlot))
        {
            return OpaaxString::Format("u_Params[{}].{}", lIndex / 4u, k_Components[lIndex % 4u]);
        }
        if (InSlot == EShaderParamSlot::Color0 || InSlot == EShaderParamSlot::Color1)
        {
            return OpaaxString::Format("u_Colors[{}]", lIndex - static_cast<Uint32>(EShaderParamSlot::Color0));
        }
        if (InSlot == EShaderParamSlot::Texture0 || InSlot == EShaderParamSlot::Texture1)
        {
            return OpaaxString::Format("u_Textures[MATERIAL_SLOT_{}]", lIndex - static_cast<Uint32>(EShaderParamSlot::Texture0));
        }
        return OpaaxString();
    }

    OpaaxString ShaderParams::InsertDefines(const OpaaxString& InStage, const ShaderParamList& InParams)
    {
        if (InParams.Params.empty())
        {
            return InStage;
        }

        OpaaxString lDefines("// The names of the shader's @param lines.\n");
        for (const ShaderParam& lParam : InParams.Params)
        {
            lDefines += OpaaxString::Format("#define {} {}\n", lParam.Name.CStr(), Expression(lParam.Slot).CStr());
        }

        // After the #version line: nothing but comments may come before it.
        const OpaaxStringView lStage(InStage);
        for (Uint32 lLineStart = 0; lLineStart < lStage.GetLength();)
        {
            const Int32  lBreak   = lStage.Find('\n', lLineStart);
            const Uint32 lLineEnd = (lBreak < 0) ? lStage.GetLength() : static_cast<Uint32>(lBreak);
            if (lStage.SubString(lLineStart, lLineEnd - lLineStart).TrimStart().StartsWith("#version"))
            {
                OpaaxString lOut(lStage.SubString(0, lLineEnd));
                lOut += '\n';
                lOut += lDefines;
                if (lLineEnd + 1 < lStage.GetLength())
                {
                    lOut += lStage.SubString(lLineEnd + 1);
                }
                return lOut;
            }
            lLineStart = lLineEnd + 1;
        }

        return lDefines + InStage;
    }
}
