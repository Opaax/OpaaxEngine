#include "SceneManager.h"

#include "SceneAsset.h"
#include "SceneSerializer.h"
#include "Assets/AssetManifest.h"
#include "Assets/AssetRegistry.h"
#include "Core/CoreEngineApp.h"
#include "Core/OpaaxPath.h"
#include "World/World.h"

namespace Opaax
{
    // =============================================================================
    // Lifecycle drive (out-of-line so we can reach the engine's shared World
    // via GetEngineApp() without dragging CoreEngineApp.h into SceneManager.h).
    // =============================================================================
    void SceneManager::Push(UniquePtr<Scene> InScene)
    {
        OPAAX_CORE_ASSERT(InScene != nullptr)
        World& lWorld = GetEngineApp()->GetWorld();

        if (!m_Stack.empty())
        {
            m_Stack.back()->OnExit();
            OPAAX_CORE_TRACE("SceneManager::Push — '{}' exited.", m_Stack.back()->GetName());
        }

        OPAAX_CORE_TRACE("SceneManager::Push — loading '{}'.", InScene->GetName());
        InScene->OnLoad(lWorld);
        InScene->OnEnter();

        m_Stack.push_back(std::move(InScene));
        SyncCurrentSceneFromActive();
    }

    void SceneManager::Pop()
    {
        if (m_Stack.empty())
        {
            OPAAX_CORE_WARN("SceneManager::Pop — stack is empty, ignored.");
            return;
        }

        World& lWorld = GetEngineApp()->GetWorld();

        OPAAX_CORE_TRACE("SceneManager::Pop — unloading '{}'.", m_Stack.back()->GetName());
        m_Stack.back()->OnExit();
        m_Stack.back()->OnUnload(lWorld);
        m_Stack.pop_back();

        if (!m_Stack.empty())
        {
            OPAAX_CORE_TRACE("SceneManager::Pop — '{}' entered.", m_Stack.back()->GetName());
            m_Stack.back()->OnEnter();
        }
    }

    void SceneManager::Replace(UniquePtr<Scene> InScene)
    {
        OPAAX_CORE_ASSERT(InScene != nullptr)
        World& lWorld = GetEngineApp()->GetWorld();

        if (!m_Stack.empty())
        {
            OPAAX_CORE_TRACE("SceneManager::Replace — unloading '{}'.", m_Stack.back()->GetName());
            m_Stack.back()->OnExit();
            m_Stack.back()->OnUnload(lWorld);
            m_Stack.pop_back();
        }

        OPAAX_CORE_TRACE("SceneManager::Replace — loading '{}'.", InScene->GetName());
        InScene->OnLoad(lWorld);
        InScene->OnEnter();

        m_Stack.push_back(std::move(InScene));
        SyncCurrentSceneFromActive();
    }

    void SceneManager::SaveCurrentSave()
    {
        OPAAX_CORE_ASSERT(GetActiveScene() != nullptr)
        GetActiveScene()->SaveScene(GetEngineApp()->GetWorld());
    }

    void SceneManager::Shutdown()
    {
        OPAAX_CORE_INFO("SceneManager::Shutdown() — clearing {} scene(s).", m_Stack.size());

        World& lWorld = GetEngineApp()->GetWorld();

        // Unload in reverse order — top scene first.
        while (!m_Stack.empty())
        {
            m_Stack.back()->OnExit();
            m_Stack.back()->OnUnload(lWorld);
            m_Stack.pop_back();
        }
    }

    // =============================================================================
    // Editor file API
    // =============================================================================
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

        if (!SceneSerializer::Serialize(*lScene, InPath, GetEngineApp()->GetWorld()))
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
        // and does not clear the target world. Step 3 narrows this Clear() to
        // entities tagged with the active scene's SceneID.
        World& lWorld = GetEngineApp()->GetWorld();
        lWorld.Clear();

        if (!SceneSerializer::Deserialize(*lScene, InPath, lWorld))
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
            GetEngineApp()->GetWorld().Clear();
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
