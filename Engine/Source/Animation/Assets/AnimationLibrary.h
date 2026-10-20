#pragma once

#include <nlohmann/json.hpp>

#include "Animation/Assets/AnimationClip.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetRef.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(AnimationLibrary);

    /** One entry of a library: the name gameplay uses, and its clip. */
    struct AnimationLibraryEntry
    {
        /** The name gameplay uses. Named after its clip's file when left empty. */
        OpaaxStringID Name;

        /** Asset-relative ("Anims/Hero_Run.opaaxasset") or a mount. */
        TAssetRef<AnimationClip> Clip;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AnimationLibraryEntry, Name, Clip)

        OPAAX_PROPERTIES(AnimationLibraryEntry,
                         OPAAX_PROP(Name).SetTooltip("The short name gameplay asks for, like \"Run\"."),
                         OPAAX_PROP(Clip).SetTooltip("The clip this name resolves to."))
    };

    // =============================================================================
    // AnimationLibrary — a character's clips under short names: gameplay asks for OPAAX_ID("Run")
    //   instead of a clip path, and gaining a clip is adding an entry. A SpriteAnimatorComponent points
    //   at one (or plays a single clip without one).
    // =============================================================================
    class AnimationLibrary final : public Asset
    {
        OPAAX_OBJECT(AnimationLibrary, Asset)

    public:
        TDynArray<AnimationLibraryEntry> Entries;

        /** Played when an animator names no clip. Empty: the first entry. */
        OpaaxStringID DefaultClip;

        OPAAX_PROPERTIES(AnimationLibrary,
                         OPAAX_PROP(Entries).SetTooltip("The clips: names resolve by first match."),
                         OPAAX_PROP(DefaultClip).SetTooltip("The clip an animator plays when it names none.\n"
                                                            "Empty: the first entry."))

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Entries.size()); }

        /**
         * The entry named InName, or nullptr. Exact match only.
         */
        const AnimationLibraryEntry* FindExact(OpaaxStringID InName) const noexcept;

        /**
         * The clip for InName. An invalid id gives the DefaultClip, then the first entry.
         * An unknown name gives nullptr (no silent fallback).
         */
        const AnimationLibraryEntry* Find(OpaaxStringID InName) const noexcept;

        //~Begin Object interface
        /**
         * Keeps the names usable: a later entry repeating a name loses it, an entry with a clip and no
         * name takes its file's name when free, and a default naming no entry is cleared.
         */
        void PostEditChange() override;
        //~End Object interface
    };
}
