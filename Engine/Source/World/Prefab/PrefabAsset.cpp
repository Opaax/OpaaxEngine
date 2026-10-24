#include "World/Prefab/PrefabAsset.h"

namespace Opaax
{
    PrefabAsset::PrefabAsset()  = default;
    PrefabAsset::~PrefabAsset() = default;

    bool PrefabAsset::LoadSource(const char* InAbsPath)
    {
        return PrefabFile::Load(OpaaxString(InAbsPath), m_Data);   // PrefabFile logged why when it fails
    }

    Uint64 PrefabAsset::GetResourceSize() const
    {
        Uint64 lBytes = sizeof(PrefabAsset);

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
