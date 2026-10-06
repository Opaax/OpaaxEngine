#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    struct MoveModeResource;

    // =============================================================================
    // MoverData — the movement modes of one kind of entity, under short names (.opaaxmover).
    //   Lets gameplay use OPAAX_ID("Fly") instead of a path. Gaining a mode means adding an entry.
    // =============================================================================

    /** One entry: the name gameplay uses, and its tuning. */
    struct MoverEntry
    {
        /** The name gameplay uses. The editor pre-fills it from the file name. */
        OpaaxStringID Name;

        /** Asset-relative ("Movers/Hero_Ground.opaaxmovemode") or a mount. */
        TResourcePath<MoveModeResource> ModeAsset;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MoverEntry, Name, ModeAsset)

        OPAAX_PROPERTIES(MoverEntry,
                         OPAAX_PROP(Name).SetTooltip("The short name gameplay asks for, like \"Fly\"."),
                         OPAAX_PROP(ModeAsset).SetTooltip("The movement tuning this name resolves to."))
    };

    /**
     * A mover: named modes, and the default mode.
     * Entries are edited by the editor panel (lists are not drawn by the property system).
     */
    struct MoverData
    {
        TDynArray<MoverEntry> Entries;

        /** Used when a mover names no mode. */
        OpaaxStringID DefaultMode;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MoverData, Entries, DefaultMode)

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Entries.size()); }

        /**
         * The entry named InName, or nullptr. Exact match only.
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
         * The mode for InName. An invalid id gives the DefaultMode, then the first entry.
         * An unknown name gives nullptr (no silent fallback).
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
