#pragma once

#include "Core/EngineAPI.h"
#include "Core/GUID/Guid.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
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
    // CameraResolution — what a world's cameras add up to this frame. Count is carried
    //   beside the view because the two log lines that matter are about it: none found,
    //   and more than one found.
    // =============================================================================
    struct CameraResolution
    {
        /** The winning camera's view, or the DEFAULT frame when Count is 0. */
        CameraView View;

        EntityID   Entity = ENTITY_NONE;
        Uint32     Count  = 0;
    };

    // =============================================================================
    // CameraManager — resolves the AUTHORED camera of the active world into the view that
    //   world is rendered with: CameraComponent -> World::SetCameraView. RendererManager
    //   reads the result and composes the matrices (Renderer/CameraView.h).
    //
    //   PLAY WORLDS ONLY, and the check is one line here rather than a world-subsystem's
    //   ShouldCreate. An Edit world is framed by the EDITOR's own camera, which writes the
    //   same slot — that is what keeps the two producers apart without the engine ever
    //   naming an editor type (D4), and why the switch is two cases and not three.
    //
    //   It is an ENGINE subsystem because there is nothing per-world to hold: the resolve
    //   is a pure function of the active world, and a camera's position lives on its entity
    //   in that world's registry. Behaviour that ticks (follow, shake) is stateless the same
    //   way, so this tier survives them arriving.
    //
    //   Several cameras: the FIRST wins and it warns. Priority is a growth point, not a
    //   field — one nothing reads is a spec (X5).
    // =============================================================================
    class OPAAX_API CameraManager final : public EngineSubsystemBase
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
         * What InWorld's cameras add up to: the winning view, which entity owns it, and how many
         * were in the running. The FIRST camera wins; a world with none answers the default frame,
         * so a caller never has to special-case emptiness.
         *
         * STATIC and pure, because that is the whole claim this subsystem rests on — resolving a
         * camera needs the world and nothing else, which is why the camera is an engine subsystem
         * with no per-world state rather than one instance per world. It also makes the rule
         * testable against a bare World, with no engine to boot.
         */
        static CameraResolution Resolve(World& InWorld);

    private:
        /**
         * Say what this frame resolved to — but only when the ANSWER CHANGED (which world, and
         * how many cameras it holds). A per-frame line would be noise and a once-ever line would
         * go quiet after the first PIE start; keyed on the world's Guid, every new world reports
         * again, which is exactly when the answer can differ.
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
        WorldManager* m_WorldManager = nullptr; // non-owning; the active world is the subject

        // What ReportResolution last said. The Guid starts INVALID, which no real world has, so
        // the first resolution of every world is always a transition.
        Guid   m_ReportedWorld;
        Uint32 m_ReportedCount = 0;
    };
}
