#pragma once

#include "Core/EngineAPI.h"
#include "Core/GUID/Guid.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Renderer/CameraView.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class World;
    class WorldManager;
    struct CameraComponent;

    OPAAX_LOG_CATEGORY(CameraManager);

    // =============================================================================
    // CameraResolution — the active world's camera result for this frame.
    // =============================================================================
    struct CameraResolution
    {
        /** The winning camera's view, or the default view when Count is 0. */
        CameraView View;

        EntityID   Entity = ENTITY_NONE;
        Uint32     Count  = 0;

        /** The cameras at the winning priority: more than one leaves the choice to storage order. */
        Uint32     Tied   = 0;
    };

    // =============================================================================
    // CameraManager — sets the view of the active Play world from its CameraComponents: the highest
    //   Priority wins (among equals, the first found, with a warning). Edit worlds use the editor's
    //   camera instead.
    // =============================================================================
    class CameraManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(CameraManager)

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        CameraManager()                                = default;
        CameraManager(const CameraManager&)            = delete;
        CameraManager& operator=(const CameraManager&) = delete;
        CameraManager(CameraManager&&)                 = delete;
        CameraManager& operator=(CameraManager&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * The view InWorld's cameras give: the highest priority wins, the first found among equals; no
         * camera gives the default view. Static and pure (testable with a bare World).
         */
        static CameraResolution Resolve(World& InWorld);

    private:
        /**
         * Logs the result only when it changes (other world, other camera count, other winner).
         */
        void ReportResolution(World& InWorld, const CameraResolution& InResolution);

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
    public:
        bool Startup()                override;
        void Shutdown()               override;
        void Update(double DeltaTime) override;
        //~End EngineSubsystemBase Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldManager* m_WorldManager = nullptr; // not owned

        // Last reported result. The Guid starts invalid, so every new world reports once.
        Guid     m_ReportedWorld;
        Uint32   m_ReportedCount  = 0;
        EntityID m_ReportedCamera = ENTITY_NONE;
    };
}
