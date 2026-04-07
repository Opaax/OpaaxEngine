#pragma once

#include "Scene.h"
#include "Core/EngineAPI.h"
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
        void Push(UniquePtr<Scene> InScene)
        {
            OPAAX_CORE_ASSERT(InScene != nullptr)

            if (!m_Stack.empty())
            {
                m_Stack.back()->OnExit();
                OPAAX_CORE_TRACE("SceneManager::Push — '{}' exited.", m_Stack.back()->GetName());
            }

            OPAAX_CORE_TRACE("SceneManager::Push — loading '{}'.", InScene->GetName());
            InScene->OnLoad();
            InScene->OnEnter();

            m_Stack.push_back(std::move(InScene));
        }

        void Pop()
        {
            if (m_Stack.empty())
            {
                OPAAX_CORE_WARN("SceneManager::Pop — stack is empty, ignored.");
                return;
            }

            OPAAX_CORE_TRACE("SceneManager::Pop — unloading '{}'.", m_Stack.back()->GetName());
            m_Stack.back()->OnExit();
            m_Stack.back()->OnUnload();
            m_Stack.pop_back();

            if (!m_Stack.empty())
            {
                OPAAX_CORE_TRACE("SceneManager::Pop — '{}' entered.", m_Stack.back()->GetName());
                m_Stack.back()->OnEnter();
            }
        }

        void Replace(UniquePtr<Scene> InScene)
        {
            OPAAX_CORE_ASSERT(InScene != nullptr)

            if (!m_Stack.empty())
            {
                OPAAX_CORE_TRACE("SceneManager::Replace — unloading '{}'.", m_Stack.back()->GetName());
                m_Stack.back()->OnExit();
                m_Stack.back()->OnUnload();
                m_Stack.pop_back();
            }

            OPAAX_CORE_TRACE("SceneManager::Replace — loading '{}'.", InScene->GetName());
            InScene->OnLoad();
            InScene->OnEnter();

            m_Stack.push_back(std::move(InScene));
        }

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

        void Shutdown() override
        {
            OPAAX_CORE_INFO("SceneManager::Shutdown() — clearing {} scene(s).", m_Stack.size());

            // Unload in reverse order — top scene first.
            while (!m_Stack.empty())
            {
                m_Stack.back()->OnExit();
                m_Stack.back()->OnUnload();
                m_Stack.pop_back();
            }
        }

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
        TDynArray<UniquePtr<Scene>> m_Stack;
    };

} // namespace Opaax