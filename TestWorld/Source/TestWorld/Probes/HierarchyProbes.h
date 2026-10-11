#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    /** Climbs the parent chain after its target. */
    struct ProbeBubble
    {
        static constexpr bool Bubbles = true;
    };

    // =============================================================================
    // PoseProbe — reads its entity's world position and its parent's name every frame.
    // =============================================================================
    class PoseProbe : public Opaax::Behaviour
    {
    public:
        float              WorldX = 0.f;
        float              WorldY = 0.f;
        Opaax::OpaaxString ParentName;

        OPAAX_PROPERTIES(PoseProbe, OPAAX_PROP(WorldX), OPAAX_PROP(WorldY), OPAAX_PROP(ParentName))

        void OnStart() override { Read(); }
        void OnUpdate(float) override { Read(); }

    private:
        void Read()
        {
            const Opaax::Vector2F lWorld = GetWorldPosition();
            WorldX     = lWorld.x;
            WorldY     = lWorld.y;
            ParentName = GetParent().GetName();
        }
    };

    // =============================================================================
    // BubbleProbe — counts the ProbeBubble events that reach its entity; with bStop they go no
    //   further up. With bSend it sends one to its own entity, once every probe has started.
    // =============================================================================
    class BubbleProbe : public Opaax::Behaviour
    {
    public:
        bool         bSend = false;
        bool         bStop = false;
        Opaax::Int32 Hits  = 0;

        OPAAX_PROPERTIES(BubbleProbe, OPAAX_PROP(bSend), OPAAX_PROP(bStop), OPAAX_PROP(Hits))

        void OnStart() override
        {
            Listen<&BubbleProbe::OnBubble>();
            if (bSend)
            {
                SetTimer<&BubbleProbe::SendBubble>(0.1f);
            }
        }

        void OnBubble(const ProbeBubble&)
        {
            ++Hits;
            if (bStop)
            {
                StopPropagation();
            }
        }

        void SendBubble() const { Send(GetEntity(), ProbeBubble{}); }
    };

    // =============================================================================
    // ReparentProbe — on start, moves its entity under the entity named NewParent (detaches it
    //   when NewParent is empty), keeping its world pose, and records where that left it.
    // =============================================================================
    class ReparentProbe : public Opaax::Behaviour
    {
    public:
        Opaax::OpaaxString NewParent;

        bool               bHasParent = false;
        Opaax::OpaaxString ParentName;
        float              LocalX = 0.f;
        float              LocalY = 0.f;

        OPAAX_PROPERTIES(ReparentProbe, OPAAX_PROP(NewParent), OPAAX_PROP(bHasParent), OPAAX_PROP(ParentName),
                         OPAAX_PROP(LocalX), OPAAX_PROP(LocalY))

        void OnStart() override
        {
            SetParent(NewParent.IsEmpty() ? Opaax::Entity{} : FindEntity(NewParent));

            const Opaax::Entity lParent = GetParent();
            bHasParent = lParent.IsValid();
            ParentName = lParent.GetName();
            LocalX     = GetPosition().x;
            LocalY     = GetPosition().y;
        }
    };

    // =============================================================================
    // MoveProbe — moves its entity to To, Delay seconds after it starts (at once for 0). To is
    //   local to the parent, or a world position with bWorldSpace.
    // =============================================================================
    class MoveProbe : public Opaax::Behaviour
    {
    public:
        float           Delay       = 0.f;
        Opaax::Vector2F To          = { 0.f, 0.f };
        bool            bWorldSpace = false;

        bool bMoved = false;

        OPAAX_PROPERTIES(MoveProbe, OPAAX_PROP(Delay), OPAAX_PROP(To), OPAAX_PROP(bWorldSpace), OPAAX_PROP(bMoved))

        void OnStart() override
        {
            if (Delay > 0.f) { SetTimer<&MoveProbe::Move>(Delay); }
            else             { Move(); }
        }

        void Move()
        {
            if (bWorldSpace) { SetWorldPosition(To); }
            else             { SetPosition(To); }
            bMoved = true;
        }
    };
}
