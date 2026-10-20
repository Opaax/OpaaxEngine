#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringJson.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetHandle.h"
#include "Resources/Asset/AssetRef.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/World.h"
#include "World/WorldSpec.h"

namespace TestWorld
{
    // =============================================================================
    // ProbeTuning — an asset class of the game (Data/Tuning.opaaxasset).
    // =============================================================================
    class ProbeTuning final : public Opaax::Asset
    {
        OPAAX_OBJECT(ProbeTuning, Opaax::Asset)

    public:
        float              Speed = 1.f;
        Opaax::Int32       Lives = 3;
        Opaax::OpaaxString Title;

        OPAAX_PROPERTIES(ProbeTuning, OPAAX_PROP(Speed), OPAAX_PROP(Lives), OPAAX_PROP(Title))
    };

    // =============================================================================
    // TunedComponent — a component of the game that points at a ProbeTuning asset.
    // =============================================================================
    struct TunedComponent
    {
        Opaax::TAssetRef<ProbeTuning> Tuning;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TunedComponent, Tuning)

        OPAAX_PROPERTIES(TunedComponent, OPAAX_PROP(Tuning))
    };

    // =============================================================================
    // ProbeCounterSubsystem — a world subsystem of the game, in Play worlds: counts its updates and
    //   fixed steps, and the entities with a TunedComponent it finds each frame.
    // =============================================================================
    class ProbeCounterSubsystem final : public Opaax::WorldSubsystem
    {
        OPAAX_OBJECT(ProbeCounterSubsystem, Opaax::WorldSubsystem)

        bool ShouldCreate(const Opaax::World& InWorld) const override { return InWorld.GetMode() == Opaax::EWorldMode::Play; }

        void Update(double) override
        {
            ++Updates;
            Tuned = 0;
            GetContext().OwningWorld.Each<TunedComponent>([this](Opaax::EntityID, const TunedComponent&) { ++Tuned; });
        }

        void FixedUpdate(double) override { ++FixedSteps; }

        Opaax::Int32 Updates    = 0;
        Opaax::Int32 FixedSteps = 0;
        Opaax::Int32 Tuned      = 0;
    };

    // =============================================================================
    // TuningProbe — loads its entity's tuning asset when it starts and copies it into its fields, and
    //   reads the game's ProbeCounterSubsystem every frame.
    // =============================================================================
    class TuningProbe : public Opaax::Behaviour
    {
    public:
        bool               bLoaded = false;
        float              Speed   = 0.f;
        Opaax::Int32       Lives   = 0;
        Opaax::OpaaxString Title;

        bool         bCounter          = false;
        Opaax::Int32 CounterUpdates    = 0;
        Opaax::Int32 CounterFixedSteps = 0;
        Opaax::Int32 CounterTuned      = 0;

        OPAAX_PROPERTIES(TuningProbe, OPAAX_PROP(bLoaded), OPAAX_PROP(Speed), OPAAX_PROP(Lives), OPAAX_PROP(Title),
                         OPAAX_PROP(bCounter), OPAAX_PROP(CounterUpdates), OPAAX_PROP(CounterFixedSteps),
                         OPAAX_PROP(CounterTuned))

        void OnStart() override
        {
            const TunedComponent* lTuned = TryGet<TunedComponent>();
            if (lTuned == nullptr)
            {
                return;
            }

            m_Tuning = Opaax::LoadAsset(GetContext().Resources, GetContext().Paths, lTuned->Tuning);
            if (const ProbeTuning* lValue = m_Tuning.Get())
            {
                bLoaded = true;
                Speed   = lValue->Speed;
                Lives   = lValue->Lives;
                Title   = lValue->Title;
            }
        }

        void OnUpdate(float) override
        {
            if (const ProbeCounterSubsystem* lCounter = GetSubsystem<ProbeCounterSubsystem>())
            {
                bCounter          = true;
                CounterUpdates    = lCounter->Updates;
                CounterFixedSteps = lCounter->FixedSteps;
                CounterTuned      = lCounter->Tuned;
            }
        }

    private:
        Opaax::TAssetHandle<ProbeTuning> m_Tuning;   // keeps the asset loaded
    };
}
