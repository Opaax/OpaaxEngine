#include "BulletStormRenderSystem.h"

#include "Renderer/Renderer2D.h"
#include "Renderer/Texture2D.h"
#include "Renderer/RenderLayer.h"
#include "Renderer/RenderTarget.hpp"
#include "World/RenderContext.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/Log/OpaaxLog.h"

#include <cmath>

using namespace Opaax;

namespace
{
    // Fully-saturated hue per index — a colour wheel so the field is visibly multi-coloured. The
    // colour is applied as a per-quad TINT (the 24 textures are plain white 1x1 objects); distinct
    // texture OBJECTS are what stress the 16-slot batcher, the tint is purely cosmetic.
    Vector4F HueColor(float InHue01)
    {
        const float lH = InHue01 * 6.f;
        const int   lI = static_cast<int>(lH) % 6;
        const float lF = lH - std::floor(lH);
        const float lQ = 1.f - lF;
        switch (lI)
        {
            case 0:  return { 1.f, lF,  0.f, 1.f };
            case 1:  return { lQ,  1.f, 0.f, 1.f };
            case 2:  return { 0.f, 1.f, lF,  1.f };
            case 3:  return { 0.f, lQ,  1.f, 1.f };
            case 4:  return { lF,  0.f, 1.f, 1.f };
            default: return { 1.f, 0.f, lQ,  1.f };
        }
    }

    // Interleave layers per submission index so consecutive quads land in different bands — the
    // arrangement the pre-M11 per-flush sort mis-ordered once the frame split into batches.
    ERenderLayer LayerForIndex(int InN)
    {
        switch (InN % 3)
        {
            case 0:  return ERenderLayer::Background;
            case 1:  return ERenderLayer::Default;
            default: return ERenderLayer::Foreground;
        }
    }
}

void BulletStormRenderSystem::EnsureTextures()
{
    if (m_TexturesReady) { return; }

    // 24 distinct white 1x1 textures via the proven solid-colour ctor (the engine's white texture
    // uses the same path on both backends). Distinct objects -> distinct slots -> batch pressure.
    // Created here (from the app's OnStartup) — the render backend is up and we are outside a frame.
    for (int i = 0; i < k_TextureCount; ++i)
    {
        m_Textures[i] = MakeUnique<Texture2D>(1u, 1u);
    }
    m_TexturesReady = true;
}

void BulletStormRenderSystem::OnRender(World& /*InWorld*/, const RenderContext& InContext)
{
    // Textures are created from the app's OnStartup (Lesson 33). Defensive skip if somehow not ready —
    // never create GPU resources mid-frame (a Vulkan upload's submit/wait collides with frame recording).
    if (!m_TexturesReady) { return; }
    ++m_Frame;
    
    // The default OrthographicCamera is centred at origin, Y-up, 1 world unit per pixel, so the
    // visible region is roughly [-W/2, W/2] x [-H/2, H/2].
    const float lW = static_cast<float>(InContext.Target.GetWidth());
    const float lH = static_cast<float>(InContext.Target.GetHeight());
    if (lW <= 0.f || lH <= 0.f) { return; }
    
    if (m_Frame == 1)   // one-shot diagnostic — confirms OnRender runs + the render-target extent
    {
        OPAAX_TRACE("[BulletStorm] first OnRender — target {}x{}, drawing {} quads + 1 marker.",
                    static_cast<int>(lW), static_cast<int>(lH), k_QuadCount);
    }
    
    // --- Marker FIRST (submission index 0): top layer (UI) + max order. The pre-M11 per-flush sort
    //     buries it behind the field (the 5000-quad batches draw after it); the M11 frame-global sort
    //     floats it to the very end -> always on top. This is the demo's correctness assertion. ---
    Renderer2D::DrawQuad({ 0.f, 0.f }, { lH * 0.18f, lH * 0.18f },
                         Vector4F(1.f, 0.f, 1.f, 1.f), 0.f, ERenderLayer::UI, 32767);
    
    // --- The field: k_QuadCount sprites, interleaved layers + textures, gentle motion ---
    const int   lCols    = 100;
    const int   lRows    = (k_QuadCount + lCols - 1) / lCols;
    const float lFieldW  = lW * 0.95f;
    const float lFieldH  = lH * 0.95f;
    const float lStepX   = lFieldW / static_cast<float>(lCols);
    const float lStepY   = lFieldH / static_cast<float>(lRows);
    const float lSize    = (lStepX < lStepY ? lStepX : lStepY) * 1.6f;   // overlap so layering shows
    const float lOriginX = -lFieldW * 0.5f + lStepX * 0.5f;
    const float lOriginY = -lFieldH * 0.5f + lStepY * 0.5f;
    const float lPhase   = static_cast<float>(m_Frame) * 0.03f;
    
    for (int n = 0; n < k_QuadCount; ++n)
    {
        const int   lCol = n % lCols;
        const int   lRow = n / lCols;
        const float lJit = std::sin(lPhase + static_cast<float>(n) * 0.25f) * lStepX * 0.35f;
        const Vector2F lPos{ lOriginX + static_cast<float>(lCol) * lStepX + lJit,
                             lOriginY + static_cast<float>(lRow) * lStepY };
    
        const int  lTexIdx = n % k_TextureCount;
        Texture2D* lTex     = m_Textures[lTexIdx].get();          // round-robin -> interleaved textures
        const Vector4F lTint = HueColor(static_cast<float>(lTexIdx) / static_cast<float>(k_TextureCount));
        Renderer2D::DrawSprite(lPos, { lSize, lSize }, *lTex,
                               lTint, 0.f, LayerForIndex(n), static_cast<Int16>(n % 3));
    }
}
