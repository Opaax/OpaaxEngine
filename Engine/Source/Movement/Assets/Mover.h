#pragma once

#include <nlohmann/json.hpp>

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Movement/Assets/MoveMode.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetRef.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(MoverAsset);

    /** One entry of a Mover: the name gameplay uses, and its tuning. */
    struct MoverEntry
    {
        /** The name gameplay uses. Named after its tuning's file when left empty. */
        OpaaxStringID Name;

        /** Asset-relative ("Movers/Hero_Ground.opaaxasset") or a mount. */
        TAssetRef<MoveMode> ModeAsset;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MoverEntry, Name, ModeAsset)

        OPAAX_PROPERTIES(MoverEntry,
                         OPAAX_PROP(Name).SetTooltip("The short name gameplay asks for, like \"Fly\"."),
                         OPAAX_PROP(ModeAsset).SetTooltip("The movement tuning this name resolves to."))
    };

    // =============================================================================
    // Mover — the movement modes of one kind of entity, under short names: gameplay asks for
    //   OPAAX_ID("Fly") instead of a path, and gaining a mode is adding an entry. A MoverComponent
    //   points at one.
    // =============================================================================
    class Mover final : public Asset
    {
        OPAAX_OBJECT(Mover, Asset)

    public:
        TDynArray<MoverEntry> Entries;

        /** Used when a mover names no mode. Empty: the first entry. */
        OpaaxStringID DefaultMode;

        OPAAX_PROPERTIES(Mover,
                         OPAAX_PROP(Entries).SetTooltip("The modes: names resolve by first match."),
                         OPAAX_PROP(DefaultMode).SetTooltip("The mode a mover starts in when it names none.\n"
                                                            "Empty: the first entry."))

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Entries.size()); }

        /**
         * The entry named InName, or nullptr. Exact match only.
         */
        const MoverEntry* FindExact(OpaaxStringID InName) const noexcept;

        /**
         * The mode for InName. An invalid id gives the DefaultMode, then the first entry.
         * An unknown name gives nullptr (no silent fallback).
         */
        const MoverEntry* Find(OpaaxStringID InName) const noexcept;

        //~Begin Object interface
        /**
         * Keeps the names usable: a later entry repeating a name loses it, an entry with a tuning and
         * no name takes its file's name when free, and a default naming no entry is cleared.
         */
        void PostEditChange() override;
        //~End Object interface
    };
}
