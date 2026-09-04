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
    struct AnimationClipResource;   // only NAMED — TResourcePath never completes its parameter

    // =============================================================================
    // AnimationLibraryData — a character's clips under SHORT names. The `.opaaxanim` payload.
    //
    //   ITS WHOLE JOB IS THE ALIAS. A clip is its own asset (AnimationClipData), reusable and
    //   individually editable; this is what lets gameplay say OPAAX_ID("Run") instead of naming
    //   "Anims/Hero_Run.opaaxclip" — so switching state is an integer compare, not a string copy.
    //
    //   A component may skip it entirely and name one clip directly (SpriteAnimatorComponent),
    //   because a spinning coin should not need two assets to exist.
    //
    //   EVERY MEMBER IS INLINE: the struct carries no OPAAX_API, so a member defined in the DLL's
    //   .cpp is unresolvable from the exe (**I6**).
    // =============================================================================

    /** One alias: the name gameplay asks for, and the clip it resolves to. */
    struct AnimationLibraryEntry
    {
        /** What gameplay writes. The editor pre-fills it from the clip's file stem (**I13**). */
        OpaaxStringID Name;

        /** Asset-relative ("Anims/Hero_Run.opaaxclip") or a mount. */
        TResourcePath<AnimationClipResource> Clip;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AnimationLibraryEntry, Name, Clip)

        OPAAX_PROPERTIES(AnimationLibraryEntry,
                         OPAAX_PROP(Name).SetTooltip("The short name gameplay asks for, like \"Run\"."),
                         OPAAX_PROP(Clip).SetTooltip("The clip this name resolves to."))
    };

    /**
     * A library: the named clips, and which one a component with no opinion plays.
     *
     * NO OPAAX_PROPERTIES for Entries — it is a TDynArray and no property drawer draws a list, the
     * same split SpriteSheetData makes for its frames. The editor folds over the SELECTED entry;
     * the list itself is the panel's UI, which is what a list has to be to be reorderable.
     */
    struct AnimationLibraryData
    {
        TDynArray<AnimationLibraryEntry> Entries;

        /** Played by a component that names no clip of its own. */
        OpaaxStringID DefaultClip;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AnimationLibraryData, Entries, DefaultClip)

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Entries.size()); }

        /**
         * The entry called InName, or nullptr. Exact — no fallback.
         *
         * Public because the editor needs it too: "does this name already exist" is what stops a
         * rename producing two entries one lookup can never tell apart.
         */
        const AnimationLibraryEntry* FindExact(const OpaaxStringID InName) const noexcept
        {
            if (!InName.IsValid())
            {
                return nullptr;
            }

            for (const AnimationLibraryEntry& lEntry : Entries)
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
         * invalid id) — the DefaultClip, then the first entry.
         *
         * A NAMED clip that is absent answers nullptr rather than falling back, so the caller can
         * say so. Silently playing a different animation for a misspelled name is exactly the
         * wrong-answer failure this codebase refuses; "I have no opinion" is a different question
         * and is the only one that gets a default.
         */
        const AnimationLibraryEntry* Find(const OpaaxStringID InName) const noexcept
        {
            if (InName.IsValid())
            {
                return FindExact(InName);
            }

            if (const AnimationLibraryEntry* lDefault = FindExact(DefaultClip))
            {
                return lDefault;
            }

            return Entries.empty() ? nullptr : &Entries[0];
        }
    };
}
