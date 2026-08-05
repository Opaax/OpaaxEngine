#include "QuadOscillatorSubsystem.h"

#include <cmath>

#include "Application/Services/ILogger.h"
#include "World/Components/DummyComponent.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    constexpr LogCategory LogQuadOscillator{"QuadOscillator"};

    constexpr float  AMPLITUDE     = 60.f;  // world units (1 unit = 1px)
    constexpr float  SPEED         = 2.f;   // radians/second
    constexpr float  PHASE_PER_QUAD = 0.8f; // so they do not move in lockstep
}

namespace Sandbox
{
    bool QuadOscillatorSubsystem::ShouldCreate(const World& InWorld)
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    bool QuadOscillatorSubsystem::Startup()
    {
        // No entities yet — see CaptureBaselines. Saying so out loud makes the ordering visible
        // in the boot log rather than something to rediscover.
        OPAAX_LOG(LogQuadOscillator, Info, "QuadOscillator started (Play world) — baselines captured on first tick");
        return true;
    }

    void QuadOscillatorSubsystem::CaptureBaselines()
    {
        World& lWorld = m_Context->OwningWorld;

        lWorld.Each<DummyComponent>([this](EntityID InEntity, const DummyComponent& InQuad)
        {
            m_Baselines.push_back(Baseline{InEntity, InQuad.Position});
        });

        OPAAX_LOG(LogQuadOscillator, Info, "Captured {} quad baseline(s)",
                  static_cast<Uint64>(m_Baselines.size()));
    }

    void QuadOscillatorSubsystem::Update(double InDeltaTime)
    {
        if (!m_bCaptured)
        {
            m_bCaptured = true;
            CaptureBaselines();
        }

        m_Elapsed += InDeltaTime;

        World&          lWorld    = m_Context->OwningWorld;
        EntityRegistry& lRegistry = lWorld.GetRegistry();

        // Drive from the BASELINES, not from a fresh view: iterating the stored list keeps each
        // quad matched to its own origin and phase even if entities are added or removed, which
        // a positional index into a view would not survive.
        for (Uint64 lIndex = 0; lIndex < m_Baselines.size(); ++lIndex)
        {
            const Baseline& lBaseline = m_Baselines[lIndex];

            if (!lWorld.IsValid(lBaseline.Entity))
            {
                continue; // destroyed since capture — skip, do not resurrect
            }

            DummyComponent* lQuad = lRegistry.try_get<DummyComponent>(lBaseline.Entity);

            if (lQuad == nullptr)
            {
                continue;
            }

            const float lPhase = static_cast<float>(lIndex) * PHASE_PER_QUAD;

            lQuad->Position.y = lBaseline.Position.y
                              + std::sin(static_cast<float>(m_Elapsed) * SPEED + lPhase) * AMPLITUDE;
        }
    }

    void QuadOscillatorSubsystem::Shutdown()
    {
        // Put the quads back where they started. Not strictly required — a Play world is thrown
        // away — but it keeps the subsystem honest about the state it mutated, which matters the
        // moment PIE clones a world instead of owning it (S4).
        World&          lWorld    = m_Context->OwningWorld;
        EntityRegistry& lRegistry = lWorld.GetRegistry();

        for (const Baseline& lBaseline : m_Baselines)
        {
            if (!lWorld.IsValid(lBaseline.Entity))
            {
                continue;
            }

            if (DummyComponent* lQuad = lRegistry.try_get<DummyComponent>(lBaseline.Entity))
            {
                lQuad->Position = lBaseline.Position;
            }
        }

        m_Baselines.clear();
        m_bCaptured = false;

        OPAAX_LOG(LogQuadOscillator, Info, "QuadOscillator shutdown");
    }
}
