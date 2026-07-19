#include "SceneAsset.h"

namespace Opaax
{
    SceneAsset::SceneAsset(const OpaaxString& InSourcePath, OpaaxStringID InAssetID)
        : m_AssetID(InAssetID)
        , m_SourcePath(InSourcePath)
        , m_State(EAssetState::Loaded)
    {
    }

    SceneAsset::~SceneAsset() = default;
}
