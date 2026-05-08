#include "SceneManager.h"

#include "SceneAsset.h"
#include "SceneSerializer.h"
#include "Assets/AssetManifest.h"
#include "Assets/AssetRegistry.h"
#include "Core/OpaaxPath.h"
#include "Core/World/World.h"

namespace Opaax
{
    bool SceneManager::Save()
    {
        if (!HasCurrentScenePath()) { return false; }
        return SaveAs(m_CurrentScenePath.CStr());
    }

    bool SceneManager::SaveAs(const char* InPath)
    {
        if (!InPath || InPath[0] == '\0')
        {
            OPAAX_CORE_WARN("SceneManager::SaveAs — null/empty path, ignored.");
            return false;
        }

        Scene* lScene = GetActiveScene();
        if (!lScene)
        {
            OPAAX_CORE_WARN("SceneManager::SaveAs — no active scene, ignored.");
            return false;
        }

        if (!SceneSerializer::Serialize(*lScene, InPath))
        {
            OPAAX_CORE_WARN("SceneManager::SaveAs — serialize failed for '{}'.", InPath);
            return false;
        }

        m_CurrentScenePath = InPath;
        OPAAX_CORE_INFO("SceneManager::SaveAs — wrote '{}'.", InPath);
        return true;
    }

    bool SceneManager::Open(const char* InPath)
    {
        if (!InPath || InPath[0] == '\0')
        {
            OPAAX_CORE_WARN("SceneManager::Open — null/empty path, ignored.");
            return false;
        }

        Scene* lScene = GetActiveScene();
        if (!lScene)
        {
            OPAAX_CORE_WARN("SceneManager::Open — no active scene, ignored.");
            return false;
        }

        // Wipe entities first; SceneSerializer::Deserialize creates new ones
        // and does not clear the target world.
        lScene->GetWorld().Clear();

        if (!SceneSerializer::Deserialize(*lScene, InPath))
        {
            OPAAX_CORE_WARN("SceneManager::Open — deserialize failed for '{}'.", InPath);
            return false;
        }

        m_CurrentScenePath = InPath;
        lScene->SetSourcePath(m_CurrentScenePath);
        OPAAX_CORE_INFO("SceneManager::Open — loaded '{}'.", InPath);
        return true;
    }

    bool SceneManager::LoadScene(OpaaxStringID InAssetID)
    {
        const AssetDescriptor* lDesc = AssetManifest::Find(InAssetID);
        if (!lDesc)
        {
            OPAAX_CORE_WARN("SceneManager::LoadScene — asset '{}' not in manifest.",
                InAssetID.ToString().CStr());
            return false;
        }

        // Drop the previous scene's descriptor so AssetRegistry / IsLoaded display
        // tracks the active scene exclusively.
        if (m_CurrentSceneAssetID.IsValid() && m_CurrentSceneAssetID != InAssetID)
        {
            AssetRegistry::Unload(m_CurrentSceneAssetID);
        }

        const TAssetHandle<SceneAsset> lHandle = AssetRegistry::Load<SceneAsset>(InAssetID);
        if (!lHandle.IsValid())
        {
            OPAAX_CORE_WARN("SceneManager::LoadScene — failed to register SceneAsset '{}'.",
                InAssetID.ToString().CStr());
            return false;
        }

        const OpaaxString lAbsPath = OpaaxPath::ToAbsolute(lDesc->RelPath);
        if (!Open(lAbsPath.CStr()))
        {
            // Open() already logs; roll back the registry entry we just added.
            AssetRegistry::Unload(InAssetID);
            return false;
        }

        m_CurrentSceneAssetID = InAssetID;
        return true;
    }

    void SceneManager::NewScene()
    {
        if (Scene* lScene = GetActiveScene())
        {
            lScene->GetWorld().Clear();
            lScene->SetSourcePath(OpaaxString{});
        }

        if (m_CurrentSceneAssetID.IsValid())
        {
            AssetRegistry::Unload(m_CurrentSceneAssetID);
            m_CurrentSceneAssetID = {};
        }

        m_CurrentScenePath.Clear();
        OPAAX_CORE_INFO("SceneManager::NewScene — world cleared.");
    }

    void SceneManager::SyncCurrentSceneFromActive()
    {
        Scene* lScene = GetActiveScene();
        if (!lScene) { return; }

        const OpaaxString& lAbsPath = lScene->GetSourcePath();
        if (lAbsPath.IsEmpty()) { return; }

        m_CurrentScenePath = lAbsPath;

        // Reverse-lookup the manifest by project-relative path. If a scene asset
        // matches, register its descriptor so AssetBrowser / type-actions speak
        // the same vocabulary as a manager-driven LoadScene.
        const OpaaxString     lRel = OpaaxPath::ToProjectRelative(lAbsPath);
        const AssetDescriptor* lDesc = AssetManifest::FindByPath(lRel);
        if (!lDesc) { return; }

        if (m_CurrentSceneAssetID.IsValid() && m_CurrentSceneAssetID != lDesc->ID)
        {
            AssetRegistry::Unload(m_CurrentSceneAssetID);
        }

        AssetRegistry::Load<SceneAsset>(lDesc->ID);
        m_CurrentSceneAssetID = lDesc->ID;
    }
}
