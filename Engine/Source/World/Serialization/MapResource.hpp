#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "World/Serialization/MapFile.h"

namespace Opaax
{
    // =============================================================================
    // MapResource — a .opaaxmap as a resource (loaded once, shared by every world).
    //   FailFast: a missing map would silently give an empty world.
    //   Does not load dependencies through LoadContext (needs IPaths for asset paths);
    //   RendererManager loads textures itself.
    // =============================================================================
    struct MapResource final
    {
        MapData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Opaax Map", MapFile::MAP_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::FailFast;

        static std::optional<MapResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            // InCtx unused (see above).
            MapResource lResource;
            if (!MapFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // MapFile already logged why
            }

            return lResource;
        }

        /**
         * Required by the concept; never used with FailFast.
         */
        static MapResource Placeholder() { return MapResource{}; }

        /**
         * Size of the entity and component records (not the JSON payloads).
         */
        Uint64 ByteSize() const noexcept
        {
            Uint64 lBytes = sizeof(MapResource);

            for (const EntityData& lEntity : Data.Entities)
            {
                lBytes += sizeof(EntityData) + lEntity.Name.GetLength();
                lBytes += static_cast<Uint64>(lEntity.Components.size()) * sizeof(ComponentData);
            }

            return lBytes;
        }
    };
}
