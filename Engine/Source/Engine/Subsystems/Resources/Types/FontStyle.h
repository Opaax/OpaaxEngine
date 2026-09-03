#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // FontStyle — the four axes a typeface is CHOSEN by: script, weight, width, slant.
    //
    //   Google Fonts' own vocabulary, deliberately, because that is where the files come from:
    //   `roboto-greek-500-italic.ttf` states three of the four in its name. Naming the axes the way
    //   the source names them is what makes "which file is this?" answerable without a table.
    //
    //   NOT under Renderer/: the layout walker never sees a style. It draws a face that has already
    //   been chosen, so this is selection vocabulary and it lives with the family that resolves it.
    //
    //   Four enums rather than four numbers, so every one of them draws as a dropdown with no editor
    //   code (**I15**) and a value outside the set is not expressible.
    // =============================================================================

    /**
     * Which SCRIPT a face covers — Google's subset names, one per file.
     *
     * The subset is a SELECTION key, not a bake instruction: `FontBake` asks the file what it
     * carries. So this says which file to reach for, and the file says what is in it.
     *
     * No CJK member, and that is the same line `FontBake::BakeParams::LastCodepoint` draws: Chinese
     * and Japanese need a dynamic atlas before they need an enumerator.
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

    OPAAX_ENUM_VALUES(EFontSubset, Latin, LatinExt, Greek, GreekExt, Cyrillic, CyrillicExt,
                      Vietnamese, Math, Symbols)

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
     * Stroke weight, on the CSS/Google scale.
     *
     * The numbers are the enumerator VALUES, not decoration: "nearest weight" — what a family falls
     * back to when it has no exact match — is then a subtraction rather than a table.
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

    OPAAX_ENUM_VALUES(EFontWeight, Thin, ExtraLight, Light, Regular, Medium, SemiBold, Bold,
                      ExtraBold, Black)

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

    /** The weight as its number, for the arithmetic the fallback ladder does. */
    inline Uint16 WeightValue(const EFontWeight InWeight) noexcept
    {
        return static_cast<Uint16>(InWeight);
    }

    /**
     * Horizontal proportion.
     *
     * NOTHING RESOLVES ANYTHING BUT `Normal` TODAY, and that is a property of the files rather than
     * of this enum: the static Roboto export carries no width axis — condensed is a SEPARATE family
     * (Roboto Condensed), or the variable font. The axis is here so that family drops in without a
     * file-format change to every `.opaaxfont` already written.
     */
    enum class EFontWidth : Uint8
    {
        Condensed,
        SemiCondensed,
        Normal,
        Expanded
    };

    OPAAX_ENUM_VALUES(EFontWidth, Condensed, SemiCondensed, Normal, Expanded)

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

    /** Upright or slanted. An enum rather than a bool so it reads as an axis beside the other three. */
    enum class EFontSlant : Uint8
    {
        Normal,
        Italic
    };

    OPAAX_ENUM_VALUES(EFontSlant, Normal, Italic)

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
     * A request: "Roboto, greek, medium, italic". What a family is asked for and what one entry
     * answers to.
     *
     * REFLECTED, so a field of this type folds into a tree node of four dropdowns with no drawer
     * written for it — the same nesting a config's Window group already uses.
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
