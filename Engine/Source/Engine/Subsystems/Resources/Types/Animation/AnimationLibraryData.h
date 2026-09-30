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
    struct AnimationClipResource;

    // =============================================================================
    // AnimationLibraryData — a character's clips under short names (.opaaxanim).
    //   Lets gameplay use OPAAX_ID("Run") instead of a clip path.
    //   A component can also name a single clip directly.
    // =============================================================================

    /** One entry: the name gameplay uses, and its clip. */
    struct AnimationLibraryEntry
    {
        /** The name gameplay uses. The editor pre-fills it from the clip's file name. */
        OpaaxStringID Name;

        /** Asset-relative ("Anims/Hero_Run.opaaxclip") or a mount. */
        TResourcePath<AnimationClipResource> Clip;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AnimationLibraryEntry, Name, Clip)

        OPAAX_PROPERTIES(AnimationLibraryEntry,
                         OPAAX_PROP(Name).SetTooltip("The short name gameplay asks for, like \"Run\"."),
                         OPAAX_PROP(Clip).SetTooltip("The clip this name resolves to."))
    };

    /**
     * A library: named clips, and the default clip.
     * Entries are edited by the editor panel (lists are not drawn by the property system).
     */
    struct AnimationLibraryData
    {
        TDynArray<AnimationLibraryEntry> Entries;

        /** Played when a component names no clip. */
        OpaaxStringID DefaultClip;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AnimationLibraryData, Entries, DefaultClip)

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Entries.size()); }

        /**
         * The entry named InName, or nullptr. Exact match only.
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
         * The clip for InName. An invalid id gives the DefaultClip, then the first entry.
         * An unknown name gives nullptr (no silent fallback).
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
