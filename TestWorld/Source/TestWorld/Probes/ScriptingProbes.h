#pragma once

#include <algorithm>

#include "Core/String/OpaaxStringJson.h"
#include "Physics/PhysicsSubsystem.h"
#include "Probes/LifecycleProbes.h"
#include "Renderer/Components/QuadComponent.h"
#include "Renderer/DebugDraw.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // HelperProbe — given to a new entity at run time by a ScriptProbe. Removes itself (only the
    //   behaviour: the entity stays) RemoveAfter seconds after it starts, reporting its end like an
    //   EndProbe.
    // =============================================================================
    class HelperProbe : public Opaax::Behaviour
    {
    public:
        float RemoveAfter = 0.2f;

        OPAAX_PROPERTIES(HelperProbe, OPAAX_PROP(RemoveAfter))

        void OnStart() override { SetTimer<&HelperProbe::Leave>(RemoveAfter); }

        void OnDestroy() override { Broadcast(ProbeEnded{ IsWorldEnding() }); }

    private:
        void Leave() { Remove<HelperProbe>(); }
    };

    // =============================================================================
    // ScriptProbe — the behaviour API a script uses most: adds, reads and later removes a component
    //   on its entity; creates an entity and gives it a HelperProbe (found and counted while it
    //   lives); a lambda timer; the clock; turns its entity; finds a world subsystem; draws a debug
    //   circle around itself.
    // =============================================================================
    class ScriptProbe : public Opaax::Behaviour
    {
    public:
        Opaax::OpaaxString Name;
        bool               bHadQuad     = false;   // before Add
        bool               bAddedQuad   = false;
        float              QuadWidth    = 0.f;     // read back with TryGet
        bool               bRemovedQuad = false;
        bool               bSawHelper   = false;   // FindBehaviour
        Opaax::Int32       HelpersSeen  = 0;       // ForEachBehaviour, at most
        Opaax::Int32       LambdaCalls  = 0;
        bool               bPhysics     = false;   // GetSubsystem
        float              Time         = 0.f;
        bool               bDeltaSeen   = false;
        float              Rotation     = 0.f;
        Opaax::Int32       LinesQueued  = 0;

        OPAAX_PROPERTIES(ScriptProbe, OPAAX_PROP(Name), OPAAX_PROP(bHadQuad), OPAAX_PROP(bAddedQuad),
                         OPAAX_PROP(QuadWidth), OPAAX_PROP(bRemovedQuad), OPAAX_PROP(bSawHelper),
                         OPAAX_PROP(HelpersSeen), OPAAX_PROP(LambdaCalls), OPAAX_PROP(bPhysics), OPAAX_PROP(Time),
                         OPAAX_PROP(bDeltaSeen), OPAAX_PROP(Rotation), OPAAX_PROP(LinesQueued))

        void OnStart() override
        {
            Name     = GetEntityName();
            bHadQuad = Has<Opaax::QuadComponent>();

            Opaax::QuadComponent& lQuad = Add<Opaax::QuadComponent>();
            lQuad.Size  = { 80.f, 40.f };
            lQuad.Color = { 0.95f, 0.75f, 0.2f, 1.f };
            bAddedQuad  = Has<Opaax::QuadComponent>();
            if (const Opaax::QuadComponent* lRead = TryGet<Opaax::QuadComponent>())
            {
                QuadWidth = lRead->Size.x;
            }

            Opaax::Entity lHelper = CreateEntity(Opaax::OpaaxString("Helper"));
            lHelper.Add<HelperProbe>();

            SetTimer(0.15f, [this]() { ++LambdaCalls; });
            SetTimer(0.4f, [this]()
            {
                Remove<Opaax::QuadComponent>();
                bRemovedQuad = !Has<Opaax::QuadComponent>();
            });

            bPhysics = (GetSubsystem<Opaax::PhysicsSubsystem>() != nullptr);
        }

        void OnUpdate(const float InDeltaTime) override
        {
            Time       = static_cast<float>(GetTime());
            bDeltaSeen = bDeltaSeen || GetDeltaTime() > 0.f;

            bSawHelper = bSawHelper || FindBehaviour<HelperProbe>() != nullptr;
            Opaax::Int32 lHelpers = 0;
            ForEachBehaviour<HelperProbe>([&lHelpers](HelperProbe&) { ++lHelpers; });
            HelpersSeen = std::max(HelpersSeen, lHelpers);

            SetRotation(GetRotation() + 90.f * InDeltaTime);
            Rotation = GetRotation();

            GetDebugDraw().DrawCircle(GetWorldPosition(), 60.f, { 1.f, 1.f, 0.f, 1.f }, 2.f);
            LinesQueued = static_cast<Opaax::Int32>(GetDebugDraw().GetLines().size());
        }
    };

    // =============================================================================
    // DieProbe — destroys its entity with Destroy(), Seconds after it starts.
    // =============================================================================
    class DieProbe : public Opaax::Behaviour
    {
    public:
        float Seconds = 0.3f;

        OPAAX_PROPERTIES(DieProbe, OPAAX_PROP(Seconds))

        void OnStart() override { SetTimer<&DieProbe::Die>(Seconds); }

    private:
        void Die() { Destroy(); }
    };
}
