#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "Physics/Collision/CollisionChannel.h"
#include "Probes/LifecycleProbes.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Behaviour/EntityEvents.h"

namespace TestWorld
{
    // =============================================================================
    // ContactProbe — counts the physics events its entity gets: solid contacts, overlaps it owns
    //   (a sensor) and overlaps it visits (another's sensor). Names the last entity involved.
    // =============================================================================
    class ContactProbe : public Opaax::Behaviour
    {
    public:
        Opaax::Int32       CollisionsBegan = 0;
        Opaax::Int32       CollisionsEnded = 0;
        Opaax::Int32       SensedBegan     = 0;
        Opaax::Int32       SensedEnded     = 0;
        Opaax::Int32       VisitsBegan     = 0;
        Opaax::Int32       VisitsEnded     = 0;
        Opaax::OpaaxString LastOther;

        OPAAX_PROPERTIES(ContactProbe, OPAAX_PROP(CollisionsBegan), OPAAX_PROP(CollisionsEnded),
                         OPAAX_PROP(SensedBegan), OPAAX_PROP(SensedEnded), OPAAX_PROP(VisitsBegan),
                         OPAAX_PROP(VisitsEnded), OPAAX_PROP(LastOther))

        void OnStart() override
        {
            Listen<&ContactProbe::OnCollisionBegan>();
            Listen<&ContactProbe::OnCollisionEnded>();
            Listen<&ContactProbe::OnOverlapBegan>();
            Listen<&ContactProbe::OnOverlapEnded>();
        }

        void OnCollisionBegan(const Opaax::CollisionBegan& InEvent)
        {
            ++CollisionsBegan;
            LastOther = InEvent.Other.GetName();
        }

        void OnCollisionEnded(const Opaax::CollisionEnded&) { ++CollisionsEnded; }

        void OnOverlapBegan(const Opaax::OverlapBegan& InEvent)
        {
            if (InEvent.bIsSensor) { ++SensedBegan; }
            else                   { ++VisitsBegan; }
            LastOther = InEvent.Other.GetName();
        }

        void OnOverlapEnded(const Opaax::OverlapEnded& InEvent)
        {
            if (InEvent.bIsSensor) { ++SensedEnded; }
            else                   { ++VisitsEnded; }
        }
    };

    // =============================================================================
    // BoundsProbe — publishes ProbeLeftBounds when its entity leaves the world bounds (the engine
    //   then destroys it, so a WitnessProbe keeps the record).
    // =============================================================================
    class BoundsProbe : public Opaax::Behaviour
    {
    public:
        void OnStart() override { Listen<&BoundsProbe::OnExited>(); }

        void OnExited(const Opaax::ExitedWorldBounds& InEvent) const { Broadcast(ProbeLeftBounds{ InEvent.LastPosition.y }); }
    };

    // =============================================================================
    // ForceProbe — pushes its body through its first Seconds of physics, at Acceleration whatever
    //   its mass (force = mass * acceleration). Counted in fixed steps: the same push at any frame
    //   rate.
    // =============================================================================
    class ForceProbe : public Opaax::Behaviour
    {
    public:
        Opaax::Vector2F Acceleration = { 0.f, 0.f };
        float           Seconds      = 0.5f;

        OPAAX_PROPERTIES(ForceProbe, OPAAX_PROP(Acceleration), OPAAX_PROP(Seconds))

        void OnFixedUpdate(const float InFixedDeltaTime) override
        {
            if (m_Pushed < Seconds)
            {
                AddForce(Acceleration * GetMass());
                m_Pushed += InFixedDeltaTime;
            }
        }

    private:
        float m_Pushed = 0.f;
    };

    // =============================================================================
    // ImpulseProbe — kicks its body once on start, at Speed whatever its mass (impulse = mass *
    //   speed), with an angular kick too; records the velocity right after.
    // =============================================================================
    class ImpulseProbe : public Opaax::Behaviour
    {
    public:
        Opaax::Vector2F Speed          = { 0.f, 0.f };
        float           AngularImpulse = 0.f;

        float VelocityX       = 0.f;
        float VelocityY       = 0.f;
        float AngularVelocity = 0.f;

        OPAAX_PROPERTIES(ImpulseProbe, OPAAX_PROP(Speed), OPAAX_PROP(AngularImpulse), OPAAX_PROP(VelocityX),
                         OPAAX_PROP(VelocityY), OPAAX_PROP(AngularVelocity))

        void OnStart() override
        {
            AddImpulse(Speed * GetMass());
            AddAngularImpulse(AngularImpulse);

            VelocityX       = GetVelocity().x;
            VelocityY       = GetVelocity().y;
            AngularVelocity = GetAngularVelocity();
        }
    };

    // =============================================================================
    // SpinProbe — sets its body turning at DegreesPerSecond, or turns it with Torque every fixed
    //   step; records its rotation and turn rate once SampleAfter seconds of physics have run.
    //   Counted in fixed steps: the same sample at any frame rate.
    // =============================================================================
    class SpinProbe : public Opaax::Behaviour
    {
    public:
        float DegreesPerSecond = 0.f;
        float Torque           = 0.f;
        float SampleAfter      = 0.25f;

        float RotationAt      = 0.f;
        float AngularVelocity = 0.f;

        OPAAX_PROPERTIES(SpinProbe, OPAAX_PROP(DegreesPerSecond), OPAAX_PROP(Torque), OPAAX_PROP(SampleAfter),
                         OPAAX_PROP(RotationAt), OPAAX_PROP(AngularVelocity))

        void OnStart() override
        {
            if (DegreesPerSecond != 0.f)
            {
                SetAngularVelocity(DegreesPerSecond);
            }
        }

        void OnFixedUpdate(const float InFixedDeltaTime) override
        {
            // Before this step: the Transform shows the steps simulated so far. Half a step of
            // margin keeps the float sum from missing the step it means.
            if (!m_bSampled && m_Simulated + 0.5f * InFixedDeltaTime >= SampleAfter)
            {
                RotationAt      = GetRotation();
                AngularVelocity = GetAngularVelocity();
                m_bSampled      = true;
            }

            if (Torque != 0.f)
            {
                AddTorque(Torque);
            }
            m_Simulated += InFixedDeltaTime;
        }

    private:
        float m_Simulated = 0.f;
        bool  m_bSampled  = false;
    };

    // =============================================================================
    // RayProbe — casts a ray Distance units down from its entity, and counts the entities with a
    //   collider within Reach of it. With bDynamicOnly both see WorldDynamic colliders only.
    // =============================================================================
    class RayProbe : public Opaax::Behaviour
    {
    public:
        float Distance     = 1000.f;
        float Reach        = 100.f;
        bool  bDynamicOnly = false;

        Opaax::OpaaxString HitName;
        float              HitY     = 0.f;
        float              NormalY  = 0.f;
        Opaax::Int32       Overlaps = 0;

        OPAAX_PROPERTIES(RayProbe, OPAAX_PROP(Distance), OPAAX_PROP(Reach), OPAAX_PROP(bDynamicOnly),
                         OPAAX_PROP(HitName), OPAAX_PROP(HitY), OPAAX_PROP(NormalY), OPAAX_PROP(Overlaps))

        void OnUpdate(float) override
        {
            const Opaax::Uint64 lChannels = bDynamicOnly ? Opaax::CategoryBit(Opaax::ECollisionChannel::WorldDynamic)
                                                         : Opaax::AllChannelsMask();
            const Opaax::Vector2F lHere = GetWorldPosition();

            const Opaax::RayHit lHit = RayCast(lHere, Opaax::Vector2F{ 0.f, -1.f }, Distance, lChannels);
            HitName = lHit.Target.GetName();
            HitY    = lHit.Point.y;
            NormalY = lHit.Normal.y;

            OverlapBox(lHere - Opaax::Vector2F{ Reach, Reach }, lHere + Opaax::Vector2F{ Reach, Reach }, m_Found, lChannels);
            Overlaps = static_cast<Opaax::Int32>(m_Found.size());
        }

    private:
        Opaax::TDynArray<Opaax::Entity> m_Found;
    };
}
