#include "World/Serialization/LevelAsset.h"

namespace Opaax
{
    LevelAsset::LevelAsset()  = default;
    LevelAsset::~LevelAsset() = default;

    bool LevelAsset::LoadSource(const char* InAbsPath)
    {
        return LevelFile::Load(OpaaxString(InAbsPath), m_Data);   // LevelFile logged why when it fails
    }

    Uint64 LevelAsset::GetResourceSize() const
    {
        Uint64 lBytes = sizeof(LevelAsset) + m_Data.Name.GetLength();

        for (const OpaaxString& lMap : m_Data.Maps)
        {
            lBytes += sizeof(OpaaxString) + lMap.GetLength();
        }

        return lBytes;
    }
}
