#pragma once

#include "Core/Event/OpaaxEvent.hpp"
#include "ECS/OpaaxEntity.hpp"

// =============================================================================
// PhysicsEvents.h
//
// Engine-side physics event classes, dispatched from PhysicsSubsystem through
// CoreEngineApp::DispatchEvent after each fixed step. Game code reacts by
// overriding GameSubsystemBase::OnEvent and opting into EEventCategory_Physics
// via GetEventCategoryFilter().
//
// Overlap (Mode==Overlap collider / sensor): Start -> Tick (per step, synthesized
//   by the engine's live overlap-pair tracking) -> Stop.
// Collision (Mode==Solid): Enter / Exit only (Box2D's native begin/end touch).
// =============================================================================

namespace Opaax
{
    // =============================================================================
    // Overlap
    // =============================================================================

    /**
     * @class OnOverlapStartEvent
     * Fired the step a Mode==Overlap collider begins overlapping another collider.
     * OverlapEntity is the overlap-collider owner (the sensor); OtherEntity is the visitor.
     */
    class OPAAX_API OnOverlapStartEvent final : public OpaaxEvent
    {
    public:
        OnOverlapStartEvent(EntityID InOverlapEntity, EntityID InOtherEntity) noexcept
            : m_OverlapEntity(InOverlapEntity), m_OtherEntity(InOtherEntity)
        {}

        FORCEINLINE EntityID GetOverlapEntity() const noexcept { return m_OverlapEntity; }
        FORCEINLINE EntityID GetOtherEntity()   const noexcept { return m_OtherEntity; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::OverlapStart)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Physics)

    private:
        EntityID m_OverlapEntity;
        EntityID m_OtherEntity;
    };

    // ---------------------------------------------------------------------------
    /**
     * @class OnOverlapTickEvent
     * Fired every fixed step a Mode==Overlap pair stays overlapping (synthesized by the
     * engine — Box2D reports only begin/end). Same payload as Start.
     */
    class OPAAX_API OnOverlapTickEvent final : public OpaaxEvent
    {
    public:
        OnOverlapTickEvent(EntityID InOverlapEntity, EntityID InOtherEntity) noexcept
            : m_OverlapEntity(InOverlapEntity), m_OtherEntity(InOtherEntity)
        {}

        FORCEINLINE EntityID GetOverlapEntity() const noexcept { return m_OverlapEntity; }
        FORCEINLINE EntityID GetOtherEntity()   const noexcept { return m_OtherEntity; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::OverlapTick)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Physics)

    private:
        EntityID m_OverlapEntity;
        EntityID m_OtherEntity;
    };

    // ---------------------------------------------------------------------------
    /**
     * @class OnOverlapStopEvent
     * Fired the step a Mode==Overlap pair stops overlapping. Same payload as Start.
     */
    class OPAAX_API OnOverlapStopEvent final : public OpaaxEvent
    {
    public:
        OnOverlapStopEvent(EntityID InOverlapEntity, EntityID InOtherEntity) noexcept
            : m_OverlapEntity(InOverlapEntity), m_OtherEntity(InOtherEntity)
        {}

        FORCEINLINE EntityID GetOverlapEntity() const noexcept { return m_OverlapEntity; }
        FORCEINLINE EntityID GetOtherEntity()   const noexcept { return m_OtherEntity; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::OverlapStop)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Physics)

    private:
        EntityID m_OverlapEntity;
        EntityID m_OtherEntity;
    };

    // =============================================================================
    // Collision (solid)
    // =============================================================================

    /**
     * @class OnCollisionEnterEvent
     * Fired the step two Mode==Solid colliders begin touching. EntityA/EntityB are the two
     * bodies (Box2D shapeIdA/shapeIdB order — stable but not semantically ordered).
     */
    class OPAAX_API OnCollisionEnterEvent final : public OpaaxEvent
    {
    public:
        OnCollisionEnterEvent(EntityID InEntityA, EntityID InEntityB) noexcept
            : m_EntityA(InEntityA), m_EntityB(InEntityB)
        {}

        FORCEINLINE EntityID GetEntityA() const noexcept { return m_EntityA; }
        FORCEINLINE EntityID GetEntityB() const noexcept { return m_EntityB; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::CollisionEnter)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Physics)

    private:
        EntityID m_EntityA;
        EntityID m_EntityB;
    };

    // ---------------------------------------------------------------------------
    /**
     * @class OnCollisionExitEvent
     * Fired the step two Mode==Solid colliders stop touching. Same payload as Enter.
     */
    class OPAAX_API OnCollisionExitEvent final : public OpaaxEvent
    {
    public:
        OnCollisionExitEvent(EntityID InEntityA, EntityID InEntityB) noexcept
            : m_EntityA(InEntityA), m_EntityB(InEntityB)
        {}

        FORCEINLINE EntityID GetEntityA() const noexcept { return m_EntityA; }
        FORCEINLINE EntityID GetEntityB() const noexcept { return m_EntityB; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::CollisionExit)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Physics)

    private:
        EntityID m_EntityA;
        EntityID m_EntityB;
    };

} // namespace Opaax
