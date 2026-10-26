#pragma once

#include <nlohmann/json.hpp>

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Renderer/Text/FontFace.h"   // complete: an asset class visits its face references
#include "Renderer/Text/FontStyle.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetRef.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(FontFamily);

    /** One face: its style, and its file. */
    struct FontFamilyEntry
    {
        FontStyleKey Style;

        /** Asset-relative ("Fonts/Roboto/roboto-latin-400-normal.ttf") or a mount. */
        TAssetRef<FontFace> Face;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(FontFamilyEntry, Style, Face)

        // Style shows as four dropdowns.
        OPAAX_PROPERTIES(FontFamilyEntry,
                         OPAAX_PROP(Style),
                         OPAAX_PROP(Face).SetTooltip("The .ttf this cut resolves to.\n"
                                                     "Drag one from the Resource Browser."))
    };

    // =============================================================================
    // FontFamily — every face of a typeface, by style: text asks for "Roboto, greek, medium, italic"
    //   instead of a .ttf path, and the family answers with its closest face. A TextComponent can also
    //   name a single .ttf directly.
    // =============================================================================
    class FontFamily final : public Asset
    {
        OPAAX_OBJECT(FontFamily, Asset)

    public:
        TDynArray<FontFamilyEntry> Entries;

        OPAAX_PROPERTIES(FontFamily,
                         OPAAX_PROP(Entries).SetTooltip("Every face of the typeface, one per style."))

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Entries.size()); }

        /** The entry matching InKey exactly, or nullptr. */
        const FontFamilyEntry* FindExact(const FontStyleKey& InKey) const noexcept;

        /**
         * The best face for InKey, or nullptr if the family has nothing for that script.
         * The script never falls back; the other axes do, in CSS order: width, then slant, then weight.
         * Compare the returned Style with InKey to know if the match was exact.
         */
        const FontFamilyEntry* Find(const FontStyleKey& InKey) const noexcept;

        /**
         * Distance between two styles in the same script. Lower is better; zero is exact.
         * Width outweighs slant, which outweighs weight.
         */
        static Uint32 MatchCost(const FontStyleKey& InCandidate, const FontStyleKey& InWanted) noexcept;

        //~Begin Object interface
        /** Warns about an entry repeating an earlier one's style: the first one wins, so it is never drawn. */
        void PostEditChange() override;
        //~End Object interface
    };
}
