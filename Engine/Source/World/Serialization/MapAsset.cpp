#include "World/Serialization/MapAsset.h"

namespace Opaax
{
    MapAsset::MapAsset()  = default;
    MapAsset::~MapAsset() = default;

    bool MapAsset::LoadSource(const char* InAbsPath)
    {
        return MapFile::Load(OpaaxString(InAbsPath), m_Data);   // MapFile logged why when it fails
    }

    Uint64 MapAsset::GetResourceSize() const
    {
        Uint64 lBytes = sizeof(MapAsset);

        for (const EntityData& lEntity : m_Data.Entities)
        {
            lBytes += sizeof(EntityData) + lEntity.Name.GetLength();
            lBytes += static_cast<Uint64>(lEntity.Components.size()) * sizeof(ComponentData);
        }

        for (const PrefabInstanceRecord& lRecord : m_Data.Instances)
        {
            lBytes += sizeof(PrefabInstanceRecord) + lRecord.Prefab.GetLength();
            lBytes += static_cast<Uint64>(lRecord.Overrides.size()) * sizeof(PrefabOverrideEntry);
        }

        return lBytes;
    }
}
