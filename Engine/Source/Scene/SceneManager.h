#pragma once

#include "Scene.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/Systems/EngineSubsystem.h"
#include "Core/Event/OpaaxEventTypes.hpp"

namespace Opaax
{
    /**
     * @class SceneManager
     *
     * Stack-based scene manager — registered as an engine subsystem.
     *
     * Only the top scene receives Update/FixedUpdate/Render.
     * Game code creates scenes and transfers ownership via Push().
     *
     * USAGE:
     * GetSubsystem<SceneManager>()->Push(MakeUnique<GameplayScene>());
     */
    class OPAAX_API SceneManager final : public EngineSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(SceneManager)

        // =============================================================================
        // CTORs - Dtor
        // =============================================================================
    public:
        SceneManager() = default;
        explicit SceneManager(CoreEngineApp* InEngineApp)
            : EngineSubsystemBase(InEngineApp)
        {}
        ~SceneManager() override = default;

        // =============================================================================
        // Copy - Delete
        // =============================================================================
        SceneManager(const SceneManager&)            = delete;
        SceneManager& operator=(const SceneManager&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
        SceneManager(SceneManager&&)                 = default;
        SceneManager& operator=(SceneManager&&)      = default;

        // =============================================================================
        // Function
        // =============================================================================
    public:

        //------------------------------------------------------------------------------
        // API
        void Push(UniquePtr<Scene> InScene);
        void Pop();
        void Replace(UniquePtr<Scene> InScene);
        void SaveCurrentSave();

        //------------------------------------------------------------------------------
        // Editor scene file API
        //
        // Save / Open route through SceneSerializer and operate on the active scene
        // (no Replace) so a derived Scene type pushed by the game is preserved.
        //
        // LoadScene(InID) is the high-level orchestrator: it owns the
        // AssetRegistry-membership invariant for scenes (only one SceneAsset is
        // live at a time), resolves the path through the manifest, swaps the
        // world, and updates current-scene tracking. Editor entry points
        // (asset-browser double-click, SceneTypeActions) route through this.

        bool Save();
        bool SaveAs(const char* InPath);
        bool Open(const char* InPath);
        bool LoadScene(OpaaxStringID InAssetID);
        void NewScene();

        //------------------------------------------------------------------------------
        // Get - Set
    public:
        FORCEINLINE Scene* GetActiveScene() noexcept
        {
            return m_Stack.empty() ? nullptr : m_Stack.back().get();
        }

        FORCEINLINE const Scene* GetActiveScene() const noexcept
        {
            return m_Stack.empty() ? nullptr : m_Stack.back().get();
        }

        FORCEINLINE bool   IsEmpty()       const noexcept { return m_Stack.empty(); }
        FORCEINLINE Uint32 GetStackDepth() const noexcept { return static_cast<Uint32>(m_Stack.size()); }

        FORCEINLINE const OpaaxString& GetCurrentScenePath()    const noexcept { return m_CurrentScenePath; }
        FORCEINLINE bool               HasCurrentScenePath()    const noexcept { return !m_CurrentScenePath.IsEmpty(); }
        FORCEINLINE OpaaxStringID      GetCurrentSceneAssetID() const noexcept { return m_CurrentSceneAssetID; }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup() override
        {
            OPAAX_CORE_INFO("SceneManager::Startup()");
            return true;
        }

        void Shutdown() override;

        void Update(double DeltaTime) override
        {
            if (!m_Stack.empty())
            {
                m_Stack.back()->OnUpdate(DeltaTime);
            }
        }

        void FixedUpdate(double FixedDeltaTime) override
        {
            if (!m_Stack.empty())
            {
                m_Stack.back()->OnFixedUpdate(FixedDeltaTime);
            }
        }

        void Render(double Alpha) override
        {
            if (!m_Stack.empty())
            {
                m_Stack.back()->OnRender(Alpha);
            }
        }

        // Forward window events to the active scene.
        Uint32 GetEventCategoryFilter() const noexcept override
        {
            return EEventCategory_None;
        }
        //~Begin EngineSubsystemBase interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Helpers
        // After OnLoad, copy the scene's GetSourcePath() into m_CurrentScenePath and
        // reverse-lookup the manifest to register the matching SceneAsset. Lets a
        // scene that boots itself from a hardcoded path (e.g. GameScene) propagate
        // its state into the manager without a back-pointer.
        void SyncCurrentSceneFromActive();

        TDynArray<UniquePtr<Scene>> m_Stack;
        OpaaxString                 m_CurrentScenePath;
        OpaaxStringID               m_CurrentSceneAssetID;
    };

} // namespace Opaax