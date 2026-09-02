#include "Renderer2D.h"

#include "RHI/IRHIDevice.h"
#include "RHI/Buffer.h"
#include "RHI/Texture.h"
#include "RHI/Shader.h"
#include "RHI/UniformBuffer.h"
#include "RHI/Pipeline.h"
#include "RHI/BindGroup.h"
#include "RHI/ICommandBuffer.h"
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
    // Batch constants — a SHADER fact and a buffer bound. The per-batch limits themselves
    //   are RenderLimits' (config-driven), which is why neither of these is one.
    // =============================================================================
    static constexpr Uint32 SHADER_TEXTURE_SLOTS = 16;      // length of u_Textures[] in Sprite.glsl
    static constexpr Uint32 MAX_BATCH_QUADS      = 65536;   // ~12 MB of vertices; the sane ceiling

    // =============================================================================
    // Vertex layout
    // =============================================================================
    struct QuadVertex
    {
        Vector3F Position;     // world space XYZ (Z = 0 for 2D)
        Vector4F Color;        // RGBA tint
        Vector2F TexCoord;     // UV
        float     TexIndex;     // texture slot index (float for shader compatibility)

        // Half-extent of the quad's HOLE, in local 0..1 space. {0,0} = solid, which is what every
        // ordinary draw passes — so an outline is a VALUE, not a second pipeline (and therefore not
        // a second flush). See MakeOutlineInnerHalf.
        Vector2F InnerHalf;
    };

    // =============================================================================
    // Renderer2DData — the pImpl. All GPU + batch state, owned by one Renderer2D instance.
    //   GPU resources are raw RHI types (ITexture2D/IShader/...), created either through the
    //   device (live path) or the global factories (transitional path) — the members are the same.
    // =============================================================================
    struct Renderer2DData
    {
        TUniquePtr<IVertexArray>   QuadVAO;
        IVertexBuffer*            QuadVBO      = nullptr;  // non-owning, owned by VAO
        TUniquePtr<IShader>        QuadShader;
        TUniquePtr<ITexture2D>     WhiteTexture;
        TUniquePtr<IUniformBuffer> CameraUBO;  // binding 1: u_ViewProjection (std140)
        TUniquePtr<IPipeline>      QuadPipeline;     // sprite pipeline (shader + layout + alpha blend)
        TUniquePtr<IBindGroup>     QuadBindGroup;    // camera UBO + 16-sampler array
        ICommandBuffer*           Cmd          = nullptr;  // active recorder, set in Begin (non-owning)

        QuadBatchLimits Limits;   // resolved from RenderLimits at Init

        // The PASS, recorded whole: four vertices, a sort key and a texture id per quad. Nothing
        // flushes while this fills, which is what lets the sort span the whole pass.
        TDynArray<QuadVertex>  PassVertices;
        TDynArray<Uint64>      PassKeys;
        TDynArray<Uint32>      PassTexIds;
        TDynArray<ITexture2D*> PassTextures;   // texture id -> texture; id 0 is the white texture

        // Emit side, reused every pass.
        TDynArray<QuadPlacement> Plan;
        TDynArray<QuadVertex>    UploadBuffer;   // one batch's vertices, sized at Init
        TDynArray<ITexture2D*>   SlotTextures;   // the current batch's samplers

        glm::mat4 ViewProjection = glm::mat4(1.f);

        // Per-FRAME counters (④). Reset by RenderSystem::BeginFrame, never by StartPass — a batch
        // split is exactly the event they exist to count.
        Renderer2DStats Stats;

        bool bLoggedSplit = false;   // one-shot: the first pass that needed more than one draw call
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
        Shutdown(); // idempotent — releases GPU handles in dependency order
    }

    // =============================================================================
    // Shared batch setup — the vertex layout, index pattern, and pipeline desc that both
    // Init paths share (only the resource *creator* differs).
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

        // What the buffers and the shader can actually honour. A configured value outside that
        // says the author expected something the frame will not do, so it is clamped LOUDLY.
        // The floor of 2 slots is white plus one texture — a batch no sprite fits in is not a limit.
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
    // Init (live path) — everything through the device; no global factory / GetBackend.
    // =============================================================================
    void Renderer2D::Init(IRHIDevice& InDevice, const RenderLimits& InLimits, const ShaderDesc& InShader)
    {
        m_Data->Limits = ResolveLimits(InLimits);

        OPAAX_LOG(LogRenderer2D, Info, "Renderer2D::Init(device) — {} quads and {} texture slots per batch",
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
        m_Data->CameraUBO    = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(glm::mat4)), 1);
        m_Data->QuadPipeline = InDevice.CreatePipeline(MakeSpritePipelineDesc(m_Data->QuadShader.get()));

        // The full sampler array, always: the shader declares u_Textures[16] whatever the batch
        // limit is, and every unused unit stays bound to the white texture.
        m_Data->QuadBindGroup = InDevice.CreateBindGroup(BindGroupLayout{ 1u, SHADER_TEXTURE_SLOTS });
        m_Data->QuadBindGroup->SetUniformBuffer(*m_Data->CameraUBO);
    }

    void Renderer2D::Shutdown()
    {
        if (!m_Data) { return; }

        OPAAX_LOG(LogRenderer2D, Info, "Renderer2D::Shutdown()");
        m_Data->QuadBindGroup.reset();
        m_Data->QuadPipeline.reset();   // before the shader it references
        m_Data->QuadVAO.reset();
        m_Data->QuadShader.reset();
        m_Data->WhiteTexture.reset();
        m_Data->CameraUBO.reset();
    }

    // =============================================================================
    // Begin / End
    // =============================================================================
    void Renderer2D::BeginPass(const RenderView& InView, ICommandBuffer& InCmd)
    {
        InCmd.SetViewport(InView.Viewport.X, InView.Viewport.Y, InView.Viewport.Width, InView.Viewport.Height);

        m_Data->Cmd            = &InCmd;
        m_Data->ViewProjection = InView.ViewProjection;

        // Bind the sprite pipeline (shader + blend); upload the view-projection to the camera UBO.
        m_Data->Cmd->BindPipeline(*m_Data->QuadPipeline);
        m_Data->CameraUBO->SetData(glm::value_ptr(m_Data->ViewProjection),
                                   static_cast<Uint32>(sizeof(glm::mat4)));
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

        // Id 0 is the white texture in every pass — an untextured quad samples it and pays no slot.
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
    // Emit — the whole pass is here, and the ORDER of the two steps is the contract: sort
    //   everything recorded, THEN cut it into batches. Cutting first is what used to let a later
    //   flush draw a Background quad over a UI one (⑥). See ARCHITECTURE.md F5.
    // =============================================================================
    void Renderer2D::EmitPass()
    {
        if (m_Data->PassKeys.empty()) { return; }

        PlanQuadBatches(m_Data->PassKeys, m_Data->PassTexIds, m_Data->Limits, m_Data->Plan);

        // Every batch starts from white, this first one included — a slot left over from the
        // PREVIOUS pass would name a texture that may not exist any more.
        m_Data->SlotTextures.assign(SHADER_TEXTURE_SLOTS, m_Data->WhiteTexture.get());

        Uint32 lBatch     = 0;
        Uint32 lQuadCount = 0;
        Uint32 lSlotCount = 1;   // slot 0 = white, bound in every batch

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

            // The slot is a property of the BATCH, so it is written here and not at record time.
            const QuadVertex* lSrc = &m_Data->PassVertices[lPlacement.QuadIndex * 4u];
            QuadVertex*       lDst = &m_Data->UploadBuffer[lQuadCount * 4u];
            for (Uint32 i = 0; i < 4u; ++i)
            {
                lDst[i]          = lSrc[i];
                lDst[i].TexIndex = static_cast<float>(lPlacement.Slot);
            }

            m_Data->SlotTextures[lPlacement.Slot] = m_Data->PassTextures[m_Data->PassTexIds[lPlacement.QuadIndex]];
            lSlotCount = std::max(lSlotCount, lPlacement.Slot + 1u);
            ++lQuadCount;
        }

        Flush(lQuadCount, lSlotCount);

        // ONE line per process, the first time a pass needs more than one draw call — the condition
        // the global sort exists for, and one a smoke run cannot read off the Stats panel.
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

        // Counted here rather than at the call sites: this is the ONE place a DrawIndexed is issued,
        // and an empty batch returns above without costing one.
        ++m_Data->Stats.DrawCalls;

        // The PEAK across the frame's batches — a max, not a sum, because the pressure that matters
        // is how close any single batch came to running out of samplers.
        if (InSlotCount > m_Data->Stats.PeakTextureSlots)
        {
            m_Data->Stats.PeakTextureSlots = InSlotCount;
        }

        const Uint32 lDataSize = InQuadCount * 4u * static_cast<Uint32>(sizeof(QuadVertex));
        m_Data->QuadVBO->SetData(m_Data->UploadBuffer.data(), lDataSize);

        // Every unit references a live texture — unused ones are the white texture, so nothing
        // dangles across batches.
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
        // Rotates an axis-aligned offset (Ox, Oy) around origin by (Cos, Sin), then translates by Center.
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
                              Int16           InOrderInLayer)
    {
        // A coloured quad IS a sprite: texture id 0 is the white texture, so the sample is
        // (1,1,1,1) and the shader's multiply leaves the tint exactly as given.
        SubmitQuad(InPosition, InSize, InColor, InRotationRad, InLayer, InOrderInLayer,
                   0u, { 0.f, 0.f }, { 1.f, 1.f });
    }

    void Renderer2D::DrawSprite(const Vector2F& InPosition,
                                const Vector2F& InSize,
                                ITexture2D&     InTexture,
                                const Vector4F& InTint,
                                float           InRotationRad,
                                ERenderLayer    InLayer,
                                Int16           InOrderInLayer,
                                const Vector2F& InUVMin,
                                const Vector2F& InUVMax)
    {
        SubmitQuad(InPosition, InSize, InTint, InRotationRad, InLayer, InOrderInLayer,
                   GetTextureId(InTexture), InUVMin, InUVMax);
    }

    Uint32 Renderer2D::GetTextureId(ITexture2D& InTexture)
    {
        // Id 0 is the white texture and is never handed out, which is why the scan starts at 1.
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
        // Per axis: the border eats InThickness off each edge, so the hole's half-extent in local
        // 0..1 space is 0.5 - thickness/size.
        //
        // CLAMPED TO [0, 0.5] AT BOTH ENDS, and both ends matter. Below 0 is an inside-out hole; a
        // zero size divides by zero. Above 0.5 is what a NEGATIVE thickness produces, and while the
        // shader happens to render it the same as 0.5 (a hole larger than the quad is still the
        // whole quad), leaving it unclamped would make the returned value stop meaning what its
        // name says. The result stays monotonic: more thickness, smaller hole, more drawn.
        // The MAGNITUDE of the size, because the hole lives in the quad's LOCAL 0..1 space and
        // mirroring does not move it. A negative size is a legal flip — SubmitQuad mirrors the
        // corners, which is how a flipped sprite mirrors its texture — but it must not be read here
        // as "no size", which would answer 0 and draw the outline SOLID.
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
                                     const Int16     InOrderInLayer)
    {
        // UNTEXTURED, and that is load-bearing rather than a simplification: the shader reads
        // v_TexCoord as the quad's LOCAL position to find the border, which only holds while the UVs
        // span the full 0..1. A textured outline sampling an atlas sub-rect would carve the hole in
        // the wrong place, so there is deliberately no overload that takes one.
        SubmitQuad(InPosition, InSize, InColor, InRotationRad, InLayer, InOrderInLayer,
                   0u, { 0.f, 0.f }, { 1.f, 1.f }, MakeOutlineInnerHalf(InSize, InThickness));
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
                                const Vector2F& InInnerHalf)
    {
        const float lHalfW = InSize.x * 0.5f;
        const float lHalfH = InSize.y * 0.5f;

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

        // Winding BL -> BR -> TR -> TL, matching the index pattern. TexIndex is left at 0 and
        // written by EmitPass: the sampler slot belongs to a batch that does not exist yet.
        const auto lPush = [&](const Vector2F& InCorner, const Vector2F& InUV)
        {
            m_Data->PassVertices.emplace_back(
                QuadVertex{ { InCorner.x, InCorner.y, 0.f }, InColor, InUV, 0.f, InInnerHalf });
        };

        lPush(lBL, { InUVMin.x, InUVMin.y });
        lPush(lBR, { InUVMax.x, InUVMin.y });
        lPush(lTR, { InUVMax.x, InUVMax.y });
        lPush(lTL, { InUVMin.x, InUVMax.y });

        // The texture rides in the key so equal-order quads group by it, which is what keeps the
        // draw call count down when many sprites share one atlas. It is the PASS's id rather than
        // a slot now — stable for the whole sort instead of only for one batch.
        m_Data->PassKeys.emplace_back(MakeSortKey(InLayer, InOrderInLayer, InTexId));
        m_Data->PassTexIds.emplace_back(InTexId);

        ++m_Data->Stats.Quads;
    }

} // namespace Opaax
