#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Object/ObjectHandle.h"
#include "Renderer/CameraView.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class World;
    class WorldManager;
    class Config_Engine;
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
    //   camera instead. The view is resolved in the render pass, after the frame's fixed steps, from
    //   the camera's display pose: it frames a body where the body is drawn.
    // =============================================================================
    class CameraManager final : public EngineSubsystem
    {
        OPAAX_OBJECT(CameraManager, EngineSubsystem)

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
         * @param InAlpha Progress through the fixed step: the camera is placed at its display pose
         *   (EntityHierarchy::DisplayTransform), as bodies are drawn. None: at its current pose.
         */
        static CameraResolution Resolve(World& InWorld, TOptional<float> InAlpha = NullOpt);

    private:
        /**
         * Logs the result only when it changes (other world, other camera count, other winner).
         */
        void ReportResolution(World& InWorld, const CameraResolution& InResolution);

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin Subsystem interface
    public:
        bool Startup()              override;
        void Shutdown()             override;
        void Render(double InAlpha) override;
        //~End Subsystem interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldManager*  m_WorldManager = nullptr; // not owned

        /** Render.bInterpolation: off frames the camera at its current pose. Not owned. */
        Config_Engine* m_EngineConfig = nullptr;

        // Last reported result. The handle starts invalid, so every new world reports once.
        ObjectHandle m_ReportedWorld;
        Uint32       m_ReportedCount  = 0;
        EntityID     m_ReportedCamera = ENTITY_NONE;
    };
}
