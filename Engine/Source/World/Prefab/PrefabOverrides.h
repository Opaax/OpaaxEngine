#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    inline constexpr LogCategory LogPrefabOverrides{"PrefabOverrides"};

    // =============================================================================
    // PrefabOverrides — what ONE instance entity changed about its template (⑦-C P3).
    //
    //   PER-PROPERTY, NOT PER-COMPONENT, and that choice is the whole point (⑦-C **K4**).
    //   A per-component override keeps the WHOLE old component when anything in it is touched, so
    //   an author who nudges a sprite's Size stops receiving the prefab's later colour change —
    //   silently, and forever. That is the bug Unity's per-property model exists to avoid.
    //
    //   IT COSTS ALMOST NOTHING HERE because `ComponentData::Payload` is already `nlohmann::json`:
    //   RFC 7386 merge-patch is in the vendored library, so per-property granularity needs no
    //   property-path addressing system — which would have fought `NLOHMANN_DEFINE_TYPE_INTRUSIVE`
    //   (**I8**) and been a subsystem of its own.
    //
    //   THE TWO CAVEATS, stated rather than discovered later:
    //     - merge-patch replaces an ARRAY wholesale; it cannot express "element 2 changed". Fine
    //       for the flat structs components are today, and the trigger to revisit is the first
    //       component with a meaningfully editable array in it.
    //     - `null` MEANS REMOVE. A component payload that legitimately stores a null value would
    //       be read as a deletion — no component does, and none should, since every field comes
    //       from a typed C++ member.
    //
    //   Pure data in, pure data out: no World, no registry, no resources. That is what lets the
    //   whole override model be gated headlessly while the gestures on top stay editor-side.
    // =============================================================================
    namespace PrefabOverrides
    {
        // ---- keys -----------------------------------------------------------
        // The patch is a json OBJECT with two optional halves, kept apart so a component can never
        // be confused with the entity's own fields.
        inline constexpr const char* KEY_COMPONENTS = "components";
        inline constexpr const char* KEY_NAME       = "name";

        /**
         * The patch that turns InTemplate into InInstance.
         *
         * Per component: `json::diff`-free — a plain merge-patch of the instance's payload over the
         * template's, so only the properties that actually differ are recorded.
         *   - a component on BOTH, identical      → absent from the patch
         *   - a component on BOTH, differing      → the changed properties only
         *   - a component ONLY on the instance    → its full payload (an added component)
         *   - a component ONLY on the template    → `null` (merge-patch's removal)
         * The entity's `Name` rides beside them under its own key, and is absent when unchanged.
         *
         * @param InIgnore A component to leave out entirely — the caller passes the
         *   `PrefabInstanceComponent`'s authoring name, since the marker is identity and the
         *   template never has one, so it would otherwise show up as an addition on every entity.
         * @return An EMPTY object when nothing differs. That is the common case and callers rely on
         *   it: an unmodified instance entity writes no patch at all.
         */
        OPAAX_API nlohmann::json Diff(const EntityData& InTemplate, const EntityData& InInstance,
                                      OpaaxStringID InIgnore);

        /**
         * Apply InPatch to InOutEntity, which must be a copy of the TEMPLATE.
         *
         * The exact inverse of Diff, and the pair round-trips: `Apply(Diff(t, i), t)` reproduces
         * `i`'s components and name. Everything Diff does not mention is left exactly as the
         * template had it — which is what makes a later edit to the prefab reach every instance
         * that did not override that property.
         *
         * Tolerant: a patch naming a component the template does not have ADDS it; a `null` removes
         * one; a malformed patch is ignored with a warning rather than throwing, because this runs
         * at map load (**MP3**).
         */
        OPAAX_API void Apply(const nlohmann::json& InPatch, EntityData& InOutEntity);

        /** @return true when InPatch would change nothing — an instance that matches its prefab. */
        OPAAX_API bool IsEmpty(const nlohmann::json& InPatch);
    }
}
