#include "SceneManager.h"

#include "SceneSerializer.h"
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
        OPAAX_CORE_INFO("SceneManager::Open — loaded '{}'.", InPath);
        return true;
    }

    void SceneManager::NewScene()
    {
        if (Scene* lScene = GetActiveScene())
        {
            lScene->GetWorld().Clear();
        }
        m_CurrentScenePath.Clear();
        OPAAX_CORE_INFO("SceneManager::NewScene — world cleared.");
    }
}
