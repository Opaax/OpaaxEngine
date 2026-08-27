#include "Systems/QuadBoundsSubsystem.h"

#include "Application/Services/ILogger.h"
#include "Renderer/DebugDraw.h"
#include "World/Components/DummyComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    constexpr LogCategory LogQuadBounds{"QuadBounds"};

    constexpr float    PADDING   = 8.f;                       // so the outline clears the quad edge
    constexpr float    THICKNESS = 2.f;
    constexpr Vector4F COLOR     = { 0.f, 1.f, 0.4f, 1.f };   // green
}

bool QuadBoundsSubsystem::ShouldCreate(const World& InWorld)
{
    return InWorld.GetMode() == EWorldMode::Edit;
}

bool QuadBoundsSubsystem::Startup()
{
    OPAAX_LOG(LogQuadBounds, Info, "QuadBounds started (Edit world) — drawing quad outlines");
    return true;
}

void QuadBoundsSubsystem::Update(double /*InDeltaTime*/)
{
    World&     lWorld = m_Context->OwningWorld;
    DebugDraw& lDebug = m_Context->Debug;

    Uint64 lDrawn = 0;

    lWorld.Each<TransformComponent, DummyComponent>(
        [&](const TransformComponent& InXf, const DummyComponent& InQuad)
        {
            lDebug.DrawBox(InXf.Position,
                           InQuad.Size + Vector2F{PADDING, PADDING},
                           COLOR, THICKNESS);
            ++lDrawn;
        });

    // Log the SUCCESS branch once, not just failures: "no errors" is equally consistent with
    // an overlay that drew nothing at all (L15). One line proves boxes were actually queued.
    if (!m_bLoggedFirstDraw && lDrawn > 0)
    {
        m_bLoggedFirstDraw = true;
        OPAAX_LOG(LogQuadBounds, Info, "QuadBounds drawing {} outline(s) per frame", lDrawn);
    }
}

void QuadBoundsSubsystem::Shutdown()
{
    // Nothing to release: immediate mode means there is no retained geometry to flush. That
    // is F4's payoff — "stop drawing" is just "stop calling", which cannot disturb another
    // producer's lines (the Unreal FlushPersistentDebugLines problem, absent by construction).
    OPAAX_LOG(LogQuadBounds, Info, "QuadBounds shutdown");
}
