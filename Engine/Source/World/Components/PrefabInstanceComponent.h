#pragma once

#include <nlohmann/json.hpp>

#include "Core/GUID/Guid.h"
#include "Core/GUID/GuidJson.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"

namespace Opaax
{
    // Only NAMED here — TResourcePath never completes its parameter — so a component header does
    // not drag the resource system into every TU that touches one.
    struct PrefabResource;

    // =============================================================================
    // PrefabInstanceComponent — the mark that says an entity CAME FROM a prefab.
    //
    //   Every entity an instance creates carries one, which is what makes an instance
    //   addressable at all: a World owns one flat registry (**WM1**), so "the entities of this
    //   instance" is a FILTER, exactly as "the entities of this map" is (**WM2**). No container,
    //   no second store, and the same reason: there is nowhere else to put it.
    //
    //   THREE FIELDS, AND EACH ANSWERS A DIFFERENT QUESTION:
    //     Prefab       — WHICH prefab (by path, the only thing that survives a rename of anything
    //                    else). What a re-apply reloads and what an override diffs against.
    //     InstanceId   — WHICH INSTANCE. Shared by every entity of one placement, so two copies
    //                    of one prefab in one map are told apart; it is also the left half of
    //                    Guid::Derive, which is what gave those copies distinct identities.
    //     TemplateGuid — WHICH ENTITY OF THE PREFAB this is. The stable, authored id, so an
    //                    override record keys by something the prefab file itself contains rather
    //                    than by a guid that only exists at runtime.
    //
    //   IDENTITY, NOT USER DATA — which is why there is no OPAAX_PROPERTIES here and the
    //   Inspector gets a custom READ-ONLY drawer instead. The generic form would render three
    //   editable fields, and retyping a guid does not re-point an instance at anything: it breaks
    //   the link silently, which is the failure class this codebase ranks worst. Re-pointing an
    //   instance is a VERB, and it does not exist yet.
    //
    //   It is an ordinary registered component in every other respect — serialized into the map
    //   like any other, so the link survives a save/load with nothing extra to write. ⑦-C P3 is
    //   what stops the entities themselves from being written out expanded beside it.
    // =============================================================================
    struct PrefabInstanceComponent
    {
        /** Asset-relative ("Prefabs/Bullet.opaaxprefab"). EMPTY means the link is broken. */
        TResourcePath<PrefabResource> Prefab;

        /** Shared by every entity of ONE placement. Invalid means the link is broken. */
        Guid InstanceId;

        /** Which entity OF the prefab this is — its id in the prefab FILE. */
        Guid TemplateGuid;

        /** Both halves of the link have to be there for it to mean anything. */
        bool IsLinked() const noexcept
        {
            return !Prefab.IsEmpty() && InstanceId.IsValid() && TemplateGuid.IsValid();
        }

        // Satisfies CComponent. _WITH_DEFAULT is the required variant, not a preference — see
        // ComponentConcept.hpp: the plain macro reads every field with at(), which throws on a
        // missing key, at BOOT, inside Level::MountAll.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PrefabInstanceComponent,
                                                    Prefab, InstanceId, TemplateGuid)
    };
}
