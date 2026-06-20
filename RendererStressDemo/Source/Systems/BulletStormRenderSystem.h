#pragma once

#include "World/IWorldSystem.h"
#include "Core/OpaaxTypes.h"
#include "Renderer/Texture2D.h"


/**
 * @class BulletStormRenderSystem
 *
 * Procedural stress drawer (no authored entities). Each frame it issues 5000+ Renderer2D draws across
 * 3 render layers and 24 distinct runtime textures in an interleaved submission order, plus a marker
 * quad at the top layer with the maximum order. With M11's frame-global sort the marker always renders
 * on top and the layers composite correctly at any quad count; the pre-M11 per-flush sort could not
 * (later flushes drew over earlier ones once the frame split past one batch).
 */

class BulletStormRenderSystem final : public Opaax::IWorldSystem
{
public:
    // Create the 24 GPU textures. Called from the app's OnStartup — after the render backend is up and
    // OUTSIDE a frame (Lesson 33): a system is constructed in OnInitialize, before RenderSubsystem::
    // Startup, so the Vulkan device/allocator aren't ready at construction time.
    void EnsureTextures();

    void OnRender(Opaax::World& InWorld, const Opaax::RenderContext& InContext) override;

private:
    static constexpr int k_TextureCount = 24;    // > 16 slots -> forces multi-batch slot pressure
    static constexpr int k_QuadCount    = 5200;  // > 5000

    Opaax::TFixedArray<Opaax::UniquePtr<Opaax::Texture2D>, k_TextureCount> m_Textures;
    bool          m_TexturesReady = false;
    Opaax::Uint64 m_Frame         = 0;   // drives gentle per-frame motion (proves the field is live)
};
