#include "QuadOscillatorSubsystem.h"

#include <cmath>

#include "Core/Log/Logger.h"
#include "Core/Profiling/Profiler.h"   // OPAAX_STAT_SCOPE
#include "World/Components/DummyComponent.h"
#include "World/Components/TransformComponent.h"
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
        // No entities yet — see CaptureBaselines.
        return true;
    }

    void QuadOscillatorSubsystem::CaptureBaselines()
    {
        World& lWorld = m_Context->OwningWorld;

        // DummyComponent is the filter; the position moved is the entity's transform.
        lWorld.Each<TransformComponent, DummyComponent>(
            [this](EntityID InEntity, const TransformComponent& InXf, const DummyComponent&)
            {
                m_Baselines.emplace_back(InEntity, InXf.Position);
            });

        OPAAX_LOG(LogQuadOscillator, Info, "Captured {} quad baseline(s)",
                  static_cast<Uint64>(m_Baselines.size()));
    }

    void QuadOscillatorSubsystem::Update(double InDeltaTime)
    {
        // One line to appear in the Stats panel (a single branch when stats are off).
        OPAAX_STAT_SCOPE("QuadOscillator");

        if (!m_bCaptured)
        {
            m_bCaptured = true;
            CaptureBaselines();
        }

        m_Elapsed += InDeltaTime;

        World&          lWorld    = m_Context->OwningWorld;
        EntityRegistry& lRegistry = lWorld.GetRegistry();

        // Driven from the stored baselines, so each quad keeps its own origin and phase even if entities
        // are added or removed.
        for (Uint64 lIndex = 0; lIndex < m_Baselines.size(); ++lIndex)
        {
            const Baseline& lBaseline = m_Baselines[lIndex];

            if (!lWorld.IsValid(lBaseline.Entity))
            {
                continue; // destroyed since capture: skip it
            }

            TransformComponent* lXf = lRegistry.try_get<TransformComponent>(lBaseline.Entity);

            if (lXf == nullptr)
            {
                continue;
            }

            const float lPhase = static_cast<float>(lIndex) * PHASE_PER_QUAD;

            lXf->Position.y = lBaseline.Position.y
                            + std::sin(static_cast<float>(m_Elapsed) * SPEED + lPhase) * AMPLITUDE;
        }
    }

    void QuadOscillatorSubsystem::Shutdown()
    {
        // Put the quads back where they started (keeps the subsystem clean about what it changed).
        World&          lWorld    = m_Context->OwningWorld;
        EntityRegistry& lRegistry = lWorld.GetRegistry();

        for (const Baseline& lBaseline : m_Baselines)
        {
            if (!lWorld.IsValid(lBaseline.Entity))
            {
                continue;
            }

            if (TransformComponent* lXf = lRegistry.try_get<TransformComponent>(lBaseline.Entity))
            {
                lXf->Position = lBaseline.Position;
            }
        }

        m_Baselines.clear();
        m_bCaptured = false;
    }
}
