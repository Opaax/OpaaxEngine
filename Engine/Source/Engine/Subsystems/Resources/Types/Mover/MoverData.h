#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"

namespace Opaax
{
    struct MoveModeResource;   // only NAMED — TResourcePath never completes its parameter

    // =============================================================================
    // MoverData — the modes one kind of thing can move in, under SHORT names. The `.opaaxmover`
    //   payload, and the LIBRARY of the mover family.
    //
    //   ITS WHOLE JOB IS THE ALIAS, exactly as AnimationLibraryData's is. A tuning is its own
    //   asset (MoveModeData), reusable and individually editable; this is what lets gameplay say
    //   OPAAX_ID("Fly") instead of naming "Movers/Hero_Fly.opaaxmovemode" — so switching how
    //   something moves is an integer compare, not a path.
    //
    //   A MOVER IS A BAG OF MODES, which is the whole design: a character that only walks has one
    //   entry, and gaining flight is adding a second — never a new component and never a subclass.
    // =============================================================================

    /** One alias: the name gameplay asks for, and the tuning it resolves to. */
    struct MoverEntry
    {
        /** What gameplay writes. The editor pre-fills it from the file stem (**I13**). */
        OpaaxStringID Name;

        /** Asset-relative ("Movers/Hero_Ground.opaaxmovemode") or a mount. */
        TResourcePath<MoveModeResource> ModeAsset;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MoverEntry, Name, ModeAsset)

        OPAAX_PROPERTIES(MoverEntry,
                         OPAAX_PROP(Name).SetTooltip("The short name gameplay asks for, like \"Fly\"."),
                         OPAAX_PROP(ModeAsset).SetTooltip("The movement tuning this name resolves to."))
    };

    /**
     * A mover: the named modes, and which one a component with no opinion starts in.
     *
     * NO OPAAX_PROPERTIES for Entries — it is a TDynArray and no property drawer draws a list, the
     * same split AnimationLibraryData and SpriteSheetData both make. The editor folds over the
     * SELECTED entry; the list itself is the panel's UI, which is what a list has to be to be
     * reorderable.
     */
    struct MoverData
    {
        TDynArray<MoverEntry> Entries;

        /** Entered by a mover that names no mode of its own. */
        OpaaxStringID DefaultMode;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MoverData, Entries, DefaultMode)

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Entries.size()); }

        /**
         * The entry called InName, or nullptr. Exact — no fallback.
         *
         * Public because the editor needs it too: "does this name already exist" is what stops a
         * rename producing two entries one lookup can never tell apart.
         */
        const MoverEntry* FindExact(const OpaaxStringID InName) const noexcept
        {
            if (!InName.IsValid())
            {
                return nullptr;
            }

            for (const MoverEntry& lEntry : Entries)
            {
                if (lEntry.Name == InName)
                {
                    return &lEntry;
                }
            }

            return nullptr;
        }

        /**
         * What InName resolves to: the entry itself, or — when the caller has NO OPINION (an
         * invalid id) — the DefaultMode, then the first entry.
         *
         * A NAMED mode that is absent answers nullptr rather than falling back, so the caller can
         * say so. Silently moving a different way for a misspelled name is the wrong-answer
         * failure this codebase refuses; "I have no opinion" is a different question and is the
         * only one that gets a default. AnimationLibraryData::Find, verbatim in shape.
         */
        const MoverEntry* Find(const OpaaxStringID InName) const noexcept
        {
            if (InName.IsValid())
            {
                return FindExact(InName);
            }

            if (const MoverEntry* lDefault = FindExact(DefaultMode))
            {
                return lDefault;
            }

            return Entries.empty() ? nullptr : &Entries[0];
        }
    };
}
