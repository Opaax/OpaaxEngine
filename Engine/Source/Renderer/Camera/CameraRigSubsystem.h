#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class CameraStage;
    class World;

    OPAAX_LOG_CATEGORY(CameraRig);

    // =============================================================================
    // CameraRigSubsystem — runs the camera rigs of a Play world: every camera's stages (CameraStage)
    //   in each fixed step, by stage (Body, Size, then Constraint; among equals, type by type in
    //   registration order), each on the pose the ones before it left, starting from where the camera
    //   is. The result is written to the camera once, its pose in the world and its OrthoSize, so a
    //   rigged camera moves in the steps only and is drawn between them, never mixed.
    // =============================================================================
    class CameraRigSubsystem final : public WorldSubsystem
    {
        OPAAX_OBJECT(CameraRigSubsystem, WorldSubsystem)

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin WorldSubsystem interface
        /** Play worlds only: stages are behaviours. */
        bool ShouldCreate(const World& InWorld) const override;

        /** After the mover (PostPhysics), so a target is seen where this step put it; before the director. */
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Presentation; }

        void FixedUpdate(double InFixedDeltaTime) override;
        //~End WorldSubsystem interface

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** InCamera's started stages, in the order they run. */
        TDynArray<CameraStage*> GetStages(EntityID InCamera) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        struct RiggedStage
        {
            EntityID     Camera = ENTITY_NONE;
            CameraStage* Stage  = nullptr;
        };

        /** This step's stages, kept to reuse the memory. */
        TDynArray<RiggedStage> m_Stages;

        /** Said once: a stage on an entity without a CameraComponent does nothing. */
        bool m_bWarnedNoCamera = false;
    };
}
