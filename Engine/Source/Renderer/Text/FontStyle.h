#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // FontStyle — the four axes that pick a face: script, weight, width, slant
    //   (named like Google Fonts files, e.g. roboto-greek-500-italic.ttf).
    // =============================================================================

    /**
     * Which script a face covers (Google Fonts subset names). No CJK yet.
     */
    enum class EFontSubset : Uint8
    {
        Latin,
        LatinExt,
        Greek,
        GreekExt,
        Cyrillic,
        CyrillicExt,
        Vietnamese,
        Math,
        Symbols
    };

}

OPAAX_ENUM_VALUES(Opaax::EFontSubset, Latin, LatinExt, Greek, GreekExt, Cyrillic, CyrillicExt,
                  Vietnamese, Math, Symbols)

namespace Opaax
{

    inline const char* ToString(const EFontSubset InSubset) noexcept
    {
        switch (InSubset)
        {
            case EFontSubset::Latin:       return "Latin";
            case EFontSubset::LatinExt:    return "LatinExt";
            case EFontSubset::Greek:       return "Greek";
            case EFontSubset::GreekExt:    return "GreekExt";
            case EFontSubset::Cyrillic:    return "Cyrillic";
            case EFontSubset::CyrillicExt: return "CyrillicExt";
            case EFontSubset::Vietnamese:  return "Vietnamese";
            case EFontSubset::Math:        return "Math";
            case EFontSubset::Symbols:     return "Symbols";
            default:                       return "Unknown";
        }
    }

    /**
     * Stroke weight (CSS scale). The values are the weights, so "nearest" is a subtraction.
     */
    enum class EFontWeight : Uint16
    {
        Thin       = 100,
        ExtraLight = 200,
        Light      = 300,
        Regular    = 400,
        Medium     = 500,
        SemiBold   = 600,
        Bold       = 700,
        ExtraBold  = 800,
        Black      = 900
    };

}

OPAAX_ENUM_VALUES(Opaax::EFontWeight, Thin, ExtraLight, Light, Regular, Medium, SemiBold, Bold,
                  ExtraBold, Black)

namespace Opaax
{

    inline const char* ToString(const EFontWeight InWeight) noexcept
    {
        switch (InWeight)
        {
            case EFontWeight::Thin:       return "Thin";
            case EFontWeight::ExtraLight: return "ExtraLight";
            case EFontWeight::Light:      return "Light";
            case EFontWeight::Regular:    return "Regular";
            case EFontWeight::Medium:     return "Medium";
            case EFontWeight::SemiBold:   return "SemiBold";
            case EFontWeight::Bold:       return "Bold";
            case EFontWeight::ExtraBold:  return "ExtraBold";
            case EFontWeight::Black:      return "Black";
            default:                      return "Unknown";
        }
    }

    /** The weight as a number. */
    inline Uint16 WeightValue(const EFontWeight InWeight) noexcept
    {
        return static_cast<Uint16>(InWeight);
    }

    /**
     * Horizontal proportion. Only Normal is used by the current font files.
     */
    enum class EFontWidth : Uint8
    {
        Condensed,
        SemiCondensed,
        Normal,
        Expanded
    };

}

OPAAX_ENUM_VALUES(Opaax::EFontWidth, Condensed, SemiCondensed, Normal, Expanded)

namespace Opaax
{

    inline const char* ToString(const EFontWidth InWidth) noexcept
    {
        switch (InWidth)
        {
            case EFontWidth::Condensed:     return "Condensed";
            case EFontWidth::SemiCondensed: return "SemiCondensed";
            case EFontWidth::Normal:        return "Normal";
            case EFontWidth::Expanded:      return "Expanded";
            default:                        return "Unknown";
        }
    }

    /** Upright or italic. */
    enum class EFontSlant : Uint8
    {
        Normal,
        Italic
    };

}

OPAAX_ENUM_VALUES(Opaax::EFontSlant, Normal, Italic)

namespace Opaax
{

    inline const char* ToString(const EFontSlant InSlant) noexcept
    {
        switch (InSlant)
        {
            case EFontSlant::Normal: return "Normal";
            case EFontSlant::Italic: return "Italic";
            default:                 return "Unknown";
        }
    }

    /**
     * A style request: "Roboto, greek, medium, italic". Reflected (four dropdowns in the editor).
     */
    struct FontStyleKey
    {
        EFontSubset Subset = EFontSubset::Latin;
        EFontWeight Weight = EFontWeight::Regular;
        EFontWidth  Width  = EFontWidth::Normal;
        EFontSlant  Slant  = EFontSlant::Normal;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(FontStyleKey, Subset, Weight, Width, Slant)

        OPAAX_PROPERTIES(FontStyleKey,
                         OPAAX_PROP(Subset).SetTooltip("Which script this face covers.\n"
                                                       "A face draws only what its file holds —\n"
                                                       "Greek text on a Latin face is a row of boxes."),
                         OPAAX_PROP(Weight).SetTooltip("Stroke weight, on the Google scale (100..900)."),
                         OPAAX_PROP(Width).SetTooltip("Horizontal proportion.\n"
                                                      "Only Normal resolves today — the static Roboto\n"
                                                      "export carries no width axis."),
                         OPAAX_PROP(Slant))

        bool operator==(const FontStyleKey& InOther) const noexcept
        {
            return Subset == InOther.Subset && Weight == InOther.Weight
                && Width  == InOther.Width  && Slant  == InOther.Slant;
        }

        bool operator!=(const FontStyleKey& InOther) const noexcept { return !(*this == InOther); }
    };
}
