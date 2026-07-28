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
    };
}
