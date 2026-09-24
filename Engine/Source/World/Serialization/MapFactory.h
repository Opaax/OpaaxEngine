#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    class World;
    class ComponentRegistry;

    inline constexpr LogCategory LogMapFactory{"MapFactory"};

    // =============================================================================
    // MapFactory — the INSTANTIATE half of the snapshot core: MapData becomes live entities
    //   in a World. The exact inverse of MapSerializer::Capture, and the pair round-trips.
    //
    //   Stateless, for the same reason MapSerializer is.
    // =============================================================================
    class OPAAX_API MapFactory
    {
    public:
        /**
         * Recreate every entity in InData inside InWorld, GUIDs preserved.
         *
         * Preserving the Guid is the whole point: entt handles are runtime-only and are never
         * assumed stable across worlds, so a Guid is the only thing an inter-entity reference
         * can survive on. That is why this goes through World::CreateEntityWithGuid rather
         * than CreateEntity.
         *
         * INSTANTIATE IS ADDITIVE — it does not clear InWorld first. Loading a second map into
         * a world that already holds one is the streaming case, and a factory that wiped the
         * world could not serve it. Callers wanting a replace call World::Clear themselves.
         *
         * An entity whose Guid is already live is skipped (World refuses it, loudly). An
         * unknown component name is skipped with a warning rather than failing the load, so a
         * map written by a build that had one extra component type still opens.
         *
         * @return How many entities were created — less than InData.EntityCount() means some
         *         were refused, and the log says which.
         */
        static Uint64 Instantiate(const MapData& InData, World& InWorld, const ComponentRegistry& InRegistry);

        /**
         * Make InWorld MATCH InData for exactly the entities InData names — the inverse of
         * MapSerializer::CaptureEntities, and what the editor's undo replays.
         *
         * INSTANTIATE CANNOT SERVE THIS, and must not learn to: it REFUSES a Guid that is already
         * live, which is right for loading a map beside one and wrong for putting one back. This
         * answers the other question, "be this again":
         *   - recreates the entity, with its Guid, when it is gone;
         *   - overwrites the identity and every component the record names — a rename is an edit
         *     like any other, so the name is written back too;
         *   - REMOVES the registered components the record does NOT name, which is what makes
         *     undoing an "Add Component" work. An essential type refuses and stays, correctly:
         *     the record names it too, because every entity carries one (**I17**).
         *
         * Entities ABSENT from InData are left alone. Destroying them belongs to the undo record,
         * which is the only thing that knows which entities an edit touched.
         *
         * BUMPS THE WORLD'S REVISION when anything was restored. Create and destroy bump on their
         * own; a component overwritten in place does not, and the editor's dirty check is gated on
         * that number (**MP5**) — so a restore that did not bump would undo the world and leave the
         * `*` claiming otherwise.
         *
         * @return How many entities were restored — fewer than InData.EntityCount() means some
         *         were refused, and the log says which.
         */
        static Uint64 Restore(const MapData& InData, World& InWorld, const ComponentRegistry& InRegistry);
    };
}
