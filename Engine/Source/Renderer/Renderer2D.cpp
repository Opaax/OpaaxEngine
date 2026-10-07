#include "Renderer/Renderer2D.h"

#include "RHI/IRHIDevice.h"
#include "RHI/Buffer.h"
#include "RHI/Texture.h"
#include "RHI/Shader.h"
#include "RHI/UniformBuffer.h"
#include "RHI/Pipeline.h"
#include "RHI/BindGroup.h"
#include "RHI/ICommandBuffer.h"
#include "Renderer/Lighting/Lighting2D.h"
#include "Renderer/Renderer2DBatchPlan.h"
#include "Renderer/Renderer2DSortKey.h"
#include "Renderer/RenderView.h"
#include "Renderer/RenderSystemDesc.h"
#include "Core/EngineAPI.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include "Core/Maths/Maths.h"

namespace Opaax
{
    // =============================================================================
    // Batch constants. The per-batch limits come from RenderLimits (config).
    // =============================================================================
    static constexpr Uint32 SHADER_TEXTURE_SLOTS = 16;      // length of u_Textures[] in Sprite.glsl
    static constexpr Uint32 SHADOW_MAP_SLOT      = SHADER_TEXTURE_SLOTS - 1;   // SHADOW_SLOT in Sprite.glsl
    static constexpr Uint32 MAX_BATCH_QUADS      = 65536;   // upper bound (~23 MB of vertices)

    // =============================================================================
    // Vertex layout
    // =============================================================================
    struct QuadVertex
    {
        Vector3F Position;     // world XYZ (Z = 0 in 2D)
        Vector4F Color;        // RGBA tint
        Vector2F TexCoord;     // UV
        float     TexIndex;     // texture slot (float for the shader)

        // Half-extent of the quad's hole, in local 0..1 space. {0,0} = solid (an outline is a value,
        // not a separate pipeline). See MakeOutlineInnerHalf.
        Vector2F InnerHalf;

        // Position of this corner inside the mask rect (0..1). MaskIndex is the mask's sampler slot,
        // or -1 for no mask.
        Vector2F MaskUV;
        float    MaskIndex;

        // Lighting (HDR passes): the normal map's slot or -1, the emissive colour and how the quad
        // takes light (w: 0 unlit, 1 lit, 2 lit and never shadowed), and the quad's rotation
        // (cos, sin) to turn its normals into the world.
        float    NormalIndex;
        Vector4F Emissive;
        Vector2F RotationCS;
    };

    // The MaxQuadsPerBatch tooltip quotes this size (four vertices a quad).
    static_assert(sizeof(QuadVertex) == 88, "QuadVertex changed: update the MaxQuadsPerBatch tooltip");

    /** The camera block of Sprite.glsl (std140). */
    struct CameraBlock
    {
        glm::mat4 ViewProjection = glm::mat4(1.f);
        glm::vec4 PassParams     = glm::vec4(0.f);   // x: 1 for a linear-colour (HDR) pass, y: 1 for occlusion
    };

    // =============================================================================
    // Renderer2DData — the pImpl: every GPU and batch resource of one Renderer2D.
    // =============================================================================
    struct Renderer2DData
    {
        TUniquePtr<IVertexArray>   QuadVAO;
        IVertexBuffer*            QuadVBO      = nullptr;  // owned by the VAO
        TUniquePtr<IShader>        QuadShader;
        TUniquePtr<ITexture2D>     WhiteTexture;
        TUniquePtr<IUniformBuffer> CameraUBO;  // binding 1: CameraBlock (std140)
        TUniquePtr<IUniformBuffer> LightsUBO;  // binding 3: LightsBlock2D (std140)
        TUniquePtr<IPipeline>      QuadPipeline;     // shader + layout + alpha blend
        TUniquePtr<IBindGroup>     QuadBindGroup;    // camera UBO + 16 samplers
        ICommandBuffer*           Cmd          = nullptr;  // set in Begin, not owned

        QuadBatchLimits Limits;   // from RenderLimits, at Init

        // The whole pass is recorded (4 vertices, a sort key and a texture id per quad) before
        // anything is drawn, so the sort covers the whole pass.
        TDynArray<QuadVertex>  PassVertices;
        TDynArray<Uint64>      PassKeys;
        TDynArray<Uint32>      PassTexIds;
        TDynArray<Uint32>      PassMaskIds;   // 0 = no mask
        TDynArray<Uint32>      PassNormalIds; // 0 = no normal map
        TDynArray<ITexture2D*> PassTextures;   // texture id -> texture; 0 = white

        // Reused every pass.
        TDynArray<QuadPlacement> Plan;
        TDynArray<QuadVertex>    UploadBuffer;   // one batch of vertices
        TDynArray<ITexture2D*>   SlotTextures;   // this batch's textures

        // The shadow map of lit passes, in the last slot. Not owned.
        ITexture2D* ShadowMap = nullptr;

        glm::mat4 ViewProjection = glm::mat4(1.f);

        // Per-frame counters. Reset by RenderSystem::BeginFrame, not per pass.
        Renderer2DStats Stats;

        bool bLoggedSplit = false;   // logged the first multi-batch pass
    };

    // =============================================================================
    // CTORS - DTORS
    // =============================================================================
    Renderer2D::Renderer2D()
        : m_Data(MakeUnique<Renderer2DData>())
    {
    }

    Renderer2D::~Renderer2D()
    {
        Shutdown(); // safe to call twice
    }

    // =============================================================================
    // Shared batch setup: vertex layout, index pattern and pipeline desc.
    // =============================================================================
    namespace
    {
        BufferLayout MakeQuadLayout()
        {
            return BufferLayout{
                { EShaderDataType::Float3 },  // Position
                { EShaderDataType::Float4 },  // Color
                { EShaderDataType::Float2 },  // TexCoord
                { EShaderDataType::Float  },  // TexIndex
                { EShaderDataType::Float2 },  // InnerHalf
                { EShaderDataType::Float2 },  // MaskUV
                { EShaderDataType::Float  },  // MaskIndex
                { EShaderDataType::Float  },  // NormalIndex
                { EShaderDataType::Float4 },  // Emissive (w: lit)
                { EShaderDataType::Float2 },  // RotationCS
            };
        }

        void FillQuadIndices(TDynArray<Uint32>& OutIndices)
        {
            Uint32 lOffset = 0;
            for (size_t i = 0; i < OutIndices.size(); i += 6)
            {
                // Two triangles per quad: 0 1 2  2 3 0
                OutIndices[i + 0] = lOffset + 0;
                OutIndices[i + 1] = lOffset + 1;
                OutIndices[i + 2] = lOffset + 2;
                OutIndices[i + 3] = lOffset + 2;
                OutIndices[i + 4] = lOffset + 3;
                OutIndices[i + 5] = lOffset + 0;
                lOffset += 4;
            }
        }

        PipelineDesc MakeSpritePipelineDesc(IShader* InShader)
        {
            PipelineDesc lDesc;
            lDesc.Shader       = InShader;
            lDesc.VertexLayout = MakeQuadLayout();
            lDesc.Blend        = EBlendMode::Alpha;
            lDesc.Topology     = EPrimitiveTopology::Triangles;
            lDesc.DebugName    = "Renderer2D::Sprite";
            return lDesc;
        }

        // Clamp the configured limits to what the buffers and shader support (with a warning).
        // At least 2 slots: white plus one texture.
        QuadBatchLimits ResolveLimits(const RenderLimits& InLimits)
        {
            QuadBatchLimits lOut;
            lOut.MaxQuads        = std::clamp(InLimits.MaxQuads, 1u, MAX_BATCH_QUADS);
            lOut.MaxTextureSlots = std::clamp(InLimits.MaxTextureSlots, 2u, SHADER_TEXTURE_SLOTS);

            if (lOut.MaxQuads != InLimits.MaxQuads || lOut.MaxTextureSlots != InLimits.MaxTextureSlots)
            {
                OPAAX_LOG(LogRenderer2D, Warn, "Batch limits clamped: {} quads / {} slots -> {} / {}",
                          InLimits.MaxQuads, InLimits.MaxTextureSlots,
                          lOut.MaxQuads, lOut.MaxTextureSlots);
            }

            return lOut;
        }
    }

    // =============================================================================
    // Init — everything created through the device.
    // =============================================================================
    void Renderer2D::Init(IRHIDevice& InDevice, const RenderLimits& InLimits, const ShaderDesc& InShader)
    {
        m_Data->Limits = ResolveLimits(InLimits);

        OPAAX_LOG(LogRenderer2D, Trace, "Renderer2D::Init(device) — {} quads and {} texture slots per batch",
                  m_Data->Limits.MaxQuads, m_Data->Limits.MaxTextureSlots);

        const Uint32 lMaxVertices = m_Data->Limits.MaxQuads * 4u;
        const Uint32 lMaxIndices  = m_Data->Limits.MaxQuads * 6u;

        m_Data->QuadVAO = InDevice.CreateVertexArray();

        TUniquePtr<IVertexBuffer> lVBO = InDevice.CreateVertexBuffer(lMaxVertices * sizeof(QuadVertex));
        lVBO->SetLayout(MakeQuadLayout());
        m_Data->QuadVBO = lVBO.get();
        m_Data->QuadVAO->AddVertexBuffer(Move(lVBO));

        TDynArray<Uint32> lIndices(lMaxIndices);
        FillQuadIndices(lIndices);
        m_Data->QuadVAO->SetIndexBuffer(InDevice.CreateIndexBuffer(lIndices.data(), lMaxIndices));

        m_Data->WhiteTexture = InDevice.CreateTexture(1u, 1u);

        m_Data->UploadBuffer.resize(lMaxVertices);
        m_Data->SlotTextures.assign(SHADER_TEXTURE_SLOTS, m_Data->WhiteTexture.get());

        m_Data->QuadShader   = InDevice.CreateShader(InShader);
        m_Data->CameraUBO    = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(CameraBlock)), 1);

        // No light yet: lit quads see a white ambient, i.e. they are drawn as is.
        m_Data->LightsUBO = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(LightsBlock2D)), 3);
        SetLighting(LightsBlock2D{});
        m_Data->QuadPipeline = InDevice.CreatePipeline(MakeSpritePipelineDesc(m_Data->QuadShader.get()));

        // Bind all 16 samplers; unused ones get the white texture.
        m_Data->QuadBindGroup = InDevice.CreateBindGroup(BindGroupLayout{ 1u, SHADER_TEXTURE_SLOTS });
        m_Data->QuadBindGroup->SetUniformBuffer(*m_Data->CameraUBO);
    }

    void Renderer2D::Shutdown()
    {
        if (!m_Data) { return; }

        m_Data->QuadBindGroup.reset();
        m_Data->QuadPipeline.reset();   // before its shader
        m_Data->QuadVAO.reset();
        m_Data->QuadShader.reset();
        m_Data->WhiteTexture.reset();
        m_Data->CameraUBO.reset();
        m_Data->LightsUBO.reset();
    }

    void Renderer2D::SetLighting(const LightsBlock2D& InLights)
    {
        if (m_Data->LightsUBO != nullptr)
        {
            m_Data->LightsUBO->SetData(&InLights, static_cast<Uint32>(sizeof(LightsBlock2D)));
        }
    }

    void Renderer2D::SetShadowMap(ITexture2D* InShadowMap) noexcept
    {
        m_Data->ShadowMap = InShadowMap;
    }

    // =============================================================================
    // Begin / End
    // =============================================================================
    void Renderer2D::BeginPass(const RenderView& InView, ICommandBuffer& InCmd)
    {
        InCmd.SetViewport(InView.Viewport.X, InView.Viewport.Y, InView.Viewport.Width, InView.Viewport.Height);

        m_Data->Cmd            = &InCmd;
        m_Data->ViewProjection = InView.ViewProjection;

        // Bind the sprite pipeline; upload the camera block.
        m_Data->Cmd->BindPipeline(*m_Data->QuadPipeline);

        CameraBlock lCamera;
        lCamera.ViewProjection = m_Data->ViewProjection;
        lCamera.PassParams.x   = InView.bLinearColor ? 1.f : 0.f;
        lCamera.PassParams.y   = InView.bOcclusion ? 1.f : 0.f;
        m_Data->CameraUBO->SetData(&lCamera, static_cast<Uint32>(sizeof(CameraBlock)));
        StartPass();
    }

    void Renderer2D::EndPass()
    {
        EmitPass();
        m_Data->Cmd = nullptr;
    }

    void Renderer2D::StartPass()
    {
        m_Data->PassVertices.clear();
        m_Data->PassKeys.clear();
        m_Data->PassTexIds.clear();
        m_Data->PassMaskIds.clear();
        m_Data->PassNormalIds.clear();

        // Id 0 is the white texture (untextured quads use it, no slot needed).
        m_Data->PassTextures.clear();
        m_Data->PassTextures.emplace_back(m_Data->WhiteTexture.get());
    }

    const Renderer2DStats& Renderer2D::GetStats() const noexcept
    {
        return m_Data->Stats;
    }

    void Renderer2D::ResetStats() noexcept
    {
        m_Data->Stats = Renderer2DStats{};
    }

    // =============================================================================
    // Emit — sort everything recorded, then split it into batches (sorting first keeps the
    //   layer order across batches).
    // =============================================================================
    void Renderer2D::EmitPass()
    {
        if (m_Data->PassKeys.empty()) { return; }

        // A shadow map keeps the last slot for itself.
        QuadBatchLimits lLimits = m_Data->Limits;
        if (m_Data->ShadowMap != nullptr)
        {
            lLimits.MaxTextureSlots = std::min(lLimits.MaxTextureSlots, SHADOW_MAP_SLOT);
        }

        PlanQuadBatches(m_Data->PassKeys, m_Data->PassTexIds, m_Data->PassMaskIds, m_Data->PassNormalIds,
                        lLimits, m_Data->Plan);

        // Every batch starts from white (a slot from the previous pass may be stale).
        m_Data->SlotTextures.assign(SHADER_TEXTURE_SLOTS, m_Data->WhiteTexture.get());

        Uint32 lBatch     = 0;
        Uint32 lQuadCount = 0;
        Uint32 lSlotCount = 1;   // slot 0 = white

        for (const QuadPlacement& lPlacement : m_Data->Plan)
        {
            if (lPlacement.Batch != lBatch)
            {
                Flush(lQuadCount, lSlotCount);

                lBatch     = lPlacement.Batch;
                lQuadCount = 0;
                lSlotCount = 1;
                m_Data->SlotTextures.assign(SHADER_TEXTURE_SLOTS, m_Data->WhiteTexture.get());
            }

            // Slots belong to the batch, so they are written here.
            const QuadVertex* lSrc = &m_Data->PassVertices[lPlacement.QuadIndex * 4u];
            QuadVertex*       lDst = &m_Data->UploadBuffer[lQuadCount * 4u];
            const Uint32 lMaskId   = m_Data->PassMaskIds[lPlacement.QuadIndex];
            const Uint32 lNormalId = m_Data->PassNormalIds[lPlacement.QuadIndex];

            // -1: none; otherwise the slot of the mask or normal map texture.
            const float lMaskIndex   = (lMaskId != 0) ? static_cast<float>(lPlacement.MaskSlot) : -1.f;
            const float lNormalIndex = (lNormalId != 0) ? static_cast<float>(lPlacement.NormalSlot) : -1.f;

            for (Uint32 i = 0; i < 4u; ++i)
            {
                lDst[i]             = lSrc[i];
                lDst[i].TexIndex    = static_cast<float>(lPlacement.Slot);
                lDst[i].MaskIndex   = lMaskIndex;
                lDst[i].NormalIndex = lNormalIndex;
            }

            m_Data->SlotTextures[lPlacement.Slot] = m_Data->PassTextures[m_Data->PassTexIds[lPlacement.QuadIndex]];
            lSlotCount = std::max(lSlotCount, lPlacement.Slot + 1u);

            if (lMaskId != 0)
            {
                m_Data->SlotTextures[lPlacement.MaskSlot] = m_Data->PassTextures[lMaskId];
                lSlotCount = std::max(lSlotCount, lPlacement.MaskSlot + 1u);
            }

            if (lNormalId != 0)
            {
                m_Data->SlotTextures[lPlacement.NormalSlot] = m_Data->PassTextures[lNormalId];
                lSlotCount = std::max(lSlotCount, lPlacement.NormalSlot + 1u);
            }
            ++lQuadCount;
        }

        Flush(lQuadCount, lSlotCount);

        // Log once, the first time a pass needs more than one draw call.
        if (!m_Data->bLoggedSplit && lBatch > 0)
        {
            OPAAX_LOG(LogRenderer2D, Info, "Pass split into {} batches for {} quads (limit {} quads / {} slots)",
                      lBatch + 1u, static_cast<Uint32>(m_Data->PassKeys.size()),
                      m_Data->Limits.MaxQuads, m_Data->Limits.MaxTextureSlots);
            m_Data->bLoggedSplit = true;
        }
    }

    void Renderer2D::Flush(const Uint32 InQuadCount, const Uint32 InSlotCount)
    {
        if (InQuadCount == 0) { return; }

        // Counted here: the only place a DrawIndexed is issued.
        ++m_Data->Stats.DrawCalls;

        // Peak sampler use of any batch (max, not sum).
        if (InSlotCount > m_Data->Stats.PeakTextureSlots)
        {
            m_Data->Stats.PeakTextureSlots = InSlotCount;
        }

        const Uint32 lDataSize = InQuadCount * 4u * static_cast<Uint32>(sizeof(QuadVertex));
        m_Data->QuadVBO->SetData(m_Data->UploadBuffer.data(), lDataSize);

        if (m_Data->ShadowMap != nullptr)
        {
            m_Data->SlotTextures[SHADOW_MAP_SLOT] = m_Data->ShadowMap;
        }

        // Unused units point at the white texture.
        for (Uint32 i = 0; i < SHADER_TEXTURE_SLOTS; ++i)
        {
            m_Data->QuadBindGroup->SetTexture(i, *m_Data->SlotTextures[i]);
        }

        m_Data->Cmd->BindBindGroup(*m_Data->QuadBindGroup);
        m_Data->Cmd->BindVertexArray(*m_Data->QuadVAO);
        m_Data->Cmd->DrawIndexed(InQuadCount * 6);
    }

    // =============================================================================
    // Draw calls
    // =============================================================================
    namespace
    {
        // Rotates an axis-aligned offset (Ox, Oy) by (Cos, Sin), then adds Center.
        FORCEINLINE Vector2F RotateOffset(const Vector2F& InCenter, float InCos, float InSin, float InOx, float InOy)
        {
            return { InCenter.x + (InCos * InOx - InSin * InOy),
                     InCenter.y + (InSin * InOx + InCos * InOy) };
        }
    }

    void Renderer2D::DrawQuad(const Vector2F& InPosition,
                              const Vector2F& InSize,
                              const Vector4F& InColor,
                              float           InRotationRad,
                              ERenderLayer    InLayer,
                              Int16           InOrderInLayer,
                              const QuadMask& InMask,
                              const QuadLighting& InLighting)
    {
        // A coloured quad samples the white texture (id 0), so the tint is kept as is.
        SubmitQuad(InPosition, InSize, InColor, InRotationRad, InLayer, InOrderInLayer,
                   0u, { 0.f, 0.f }, { 1.f, 1.f }, { 0.f, 0.f }, InMask, InLighting);
    }

    void Renderer2D::DrawSprite(const Vector2F& InPosition,
                                const Vector2F& InSize,
                                ITexture2D&     InTexture,
                                const Vector4F& InTint,
                                float           InRotationRad,
                                ERenderLayer    InLayer,
                                Int16           InOrderInLayer,
                                const Vector2F& InUVMin,
                                const Vector2F& InUVMax,
                                const QuadMask& InMask,
                                const QuadLighting& InLighting)
    {
        SubmitQuad(InPosition, InSize, InTint, InRotationRad, InLayer, InOrderInLayer,
                   GetTextureId(InTexture), InUVMin, InUVMax, { 0.f, 0.f }, InMask, InLighting);
    }

    Uint32 Renderer2D::GetTextureId(ITexture2D& InTexture)
    {
        // Id 0 is the white texture, so the search starts at 1.
        for (Uint32 i = 1; i < static_cast<Uint32>(m_Data->PassTextures.size()); ++i)
        {
            if (m_Data->PassTextures[i] == &InTexture)
            {
                return i;
            }
        }

        m_Data->PassTextures.emplace_back(&InTexture);

        return static_cast<Uint32>(m_Data->PassTextures.size()) - 1u;
    }

    Vector2F MakeOutlineInnerHalf(const Vector2F& InSize, const float InThickness) noexcept
    {
        // Per axis, the hole's half-extent in local 0..1 space is 0.5 - thickness/size, clamped to
        // [0, 0.5]. Uses the size's magnitude: a negative size (flip) must not make it solid.
        const auto lInnerHalf = [](const float InExtent, const float InBorder) noexcept
        {
            const float lMagnitude = InExtent < 0.f ? -InExtent : InExtent;

            if (lMagnitude <= 0.f) { return 0.f; }

            const float lHalf = 0.5f - InBorder / lMagnitude;

            return lHalf < 0.f ? 0.f : (lHalf > 0.5f ? 0.5f : lHalf);
        };

        return { lInnerHalf(InSize.x, InThickness), lInnerHalf(InSize.y, InThickness) };
    }

    void Renderer2D::DrawQuadOutline(const Vector2F& InPosition,
                                     const Vector2F& InSize,
                                     const Vector4F& InColor,
                                     const float     InThickness,
                                     const float     InRotationRad,
                                     const ERenderLayer InLayer,
                                     const Int16     InOrderInLayer,
                                     const QuadMask& InMask,
                                     const QuadLighting& InLighting)
    {
        // Untextured on purpose: the shader uses the UVs as local position to find the border,
        // which only works when they span 0..1.
        SubmitQuad(InPosition, InSize, InColor, InRotationRad, InLayer, InOrderInLayer,
                   0u, { 0.f, 0.f }, { 1.f, 1.f }, MakeOutlineInnerHalf(InSize, InThickness), InMask, InLighting);
    }

    void Renderer2D::SubmitQuad(const Vector2F& InPosition,
                                const Vector2F& InSize,
                                const Vector4F& InColor,
                                float           InRotationRad,
                                ERenderLayer    InLayer,
                                Int16           InOrderInLayer,
                                Uint32          InTexId,
                                const Vector2F& InUVMin,
                                const Vector2F& InUVMax,
                                const Vector2F& InInnerHalf,
                                const QuadMask& InMask,
                                const QuadLighting& InLighting)
    {
        const float lHalfW = InSize.x * 0.5f;
        const float lHalfH = InSize.y * 0.5f;

        // Normals turn with the quad.
        const Vector2F lRotationCS = (InRotationRad == 0.f)
                                         ? Vector2F{ 1.f, 0.f }
                                         : Vector2F{ Maths::Cos(InRotationRad), Maths::Sin(InRotationRad) };
        // w: 0 unlit, 1 lit, 2 lit but never shadowed.
        const float    lLitCode = InLighting.bLit ? (InLighting.bReceiveShadows ? 1.f : 2.f) : 0.f;
        const Vector4F lEmissive{ InLighting.Emissive.x, InLighting.Emissive.y, InLighting.Emissive.z, lLitCode };

        Vector2F lBL, lBR, lTR, lTL;
        if (InRotationRad == 0.f)
        {
            lBL = { InPosition.x - lHalfW, InPosition.y - lHalfH };
            lBR = { InPosition.x + lHalfW, InPosition.y - lHalfH };
            lTR = { InPosition.x + lHalfW, InPosition.y + lHalfH };
            lTL = { InPosition.x - lHalfW, InPosition.y + lHalfH };
        }
        else
        {
            const float lCos = Maths::Cos(InRotationRad);
            const float lSin = Maths::Sin(InRotationRad);
            lBL = RotateOffset(InPosition, lCos, lSin, -lHalfW, -lHalfH);
            lBR = RotateOffset(InPosition, lCos, lSin, +lHalfW, -lHalfH);
            lTR = RotateOffset(InPosition, lCos, lSin, +lHalfW, +lHalfH);
            lTL = RotateOffset(InPosition, lCos, lSin, -lHalfW, +lHalfH);
        }

        // The mask rect in the same space as the corners; outside 0..1 the fragment is discarded.
        const bool     lMasked  = InMask.IsActive();
        const Vector2F lMaskMin = lMasked ? InMask.Rect.Min() : Vector2F{ 0.f, 0.f };
        const Vector2F lMaskSize = lMasked ? InMask.Rect.Size() : Vector2F{ 1.f, 1.f };

        // Winding BL -> BR -> TR -> TL. TexIndex and MaskIndex are set in EmitPass (per batch).
        const auto lPush = [&](const Vector2F& InCorner, const Vector2F& InUV)
        {
            const Vector2F lMaskUV{ (InCorner.x - lMaskMin.x) / lMaskSize.x,
                                    (InCorner.y - lMaskMin.y) / lMaskSize.y };

            m_Data->PassVertices.emplace_back(
                QuadVertex{ { InCorner.x, InCorner.y, 0.f }, InColor, InUV, 0.f, InInnerHalf, lMaskUV, -1.f,
                            -1.f, lEmissive, lRotationCS });
        };

        lPush(lBL, { InUVMin.x, InUVMin.y });
        lPush(lBR, { InUVMax.x, InUVMin.y });
        lPush(lTR, { InUVMax.x, InUVMax.y });
        lPush(lTL, { InUVMin.x, InUVMax.y });

        // The texture is part of the sort key, so equal-order quads group by texture (fewer draw calls).
        m_Data->PassKeys.emplace_back(MakeSortKey(InLayer, InOrderInLayer, InTexId));
        m_Data->PassTexIds.emplace_back(InTexId);

        // Id 0 means no mask, so a rect-only mask uses another id that resolves to the white texture
        // (all rect-only masks share it).
        m_Data->PassMaskIds.emplace_back(
            lMasked ? GetTextureId(InMask.Texture != nullptr ? *InMask.Texture : *m_Data->WhiteTexture) : 0u);

        m_Data->PassNormalIds.emplace_back(InLighting.NormalMap != nullptr ? GetTextureId(*InLighting.NormalMap) : 0u);

        ++m_Data->Stats.Quads;
    }

} // namespace Opaax
