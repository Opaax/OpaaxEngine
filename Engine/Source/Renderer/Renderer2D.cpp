#include "Renderer2D.h"
#include "RHI/RenderCommand.h"
#include "RHI/Buffer.h"
#include "RHI/UniformBuffer.h"
#include "RHI/Pipeline.h"
#include "RHI/BindGroup.h"
#include "RHI/ICommandBuffer.h"
#include "Renderer/ShaderAsset.h"
#include "Renderer/Texture2D.h"
#include "Renderer/Renderer2DSortKey.h"
#include "Renderer/FrameBatcher.h"
#include "Renderer/Camera/ICamera.h"
#include "Core/Config/EngineConfig.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/EngineAPI.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace Opaax
{
    // =============================================================================
    // Batch constants
    // =============================================================================
    static constexpr Uint32 MAX_QUADS         = 1000; // per-BATCH emission cap (not a frame cap)
    static constexpr Uint32 MAX_VERTICES      = MAX_QUADS * 4;
    static constexpr Uint32 MAX_INDICES       = MAX_QUADS * 6;
    static constexpr Uint32 MAX_TEXTURE_SLOTS = 16;   // minimum guaranteed by OpenGL 3.3
    
    // =============================================================================
    // Vertex layout
    // =============================================================================
    struct QuadVertex
    {
        Vector3F Position;     // world space XYZ (Z = 0 for 2D)
        Vector4F Color;        // RGBA tint
        Vector2F TexCoord;     // UV
        float    TexIndex;     // texture slot index (float for shader compatibility)
    };
    
    // =============================================================================
    // Recorded draw command
    //
    // One per DrawQuad/DrawSprite. Corners/UV/color are computed at record time; the texture SLOT is
    // resolved later, at emit, so the recorded TexIndex is a placeholder overwritten in EmitFrame.
    // =============================================================================
    struct QuadCommand
    {
        Uint64     SortKey;       // MakeSortKey(Layer, OrderInLayer, 0) — slot is NOT part of the key
        Texture2D* Texture;       // nullptr => white (slot 0)
        QuadVertex Vertices[4];
    };
 
    // =============================================================================
    // Renderer2D internal state
    // =============================================================================
    struct Renderer2DData
    {
        UniquePtr<IVertexArray>   QuadVAO;
        IVertexBuffer*            QuadVBO      = nullptr;  // non-owning, owned by VAO
        UniquePtr<ShaderAsset>    QuadShader;
        UniquePtr<Texture2D>      WhiteTexture;
        UniquePtr<IUniformBuffer> CameraUBO;  // binding 1: u_ViewProjection (std140)
        UniquePtr<IPipeline>      QuadPipeline;     // sprite pipeline (shader + layout + alpha blend)
        UniquePtr<IBindGroup>     QuadBindGroup;    // camera UBO + 16-sampler array
        ICommandBuffer*           Cmd          = nullptr;  // active recorder, set in Begin (non-owning)

        // Frame-wide draw record (persistent capacity, cleared each Begin — never freed).
        TDynArray<QuadCommand>    Commands;

        // Reused scratch for the frame-global sort + pure batch assignment (resized to N each frame).
        TDynArray<Uint32>          SortIndices;
        TDynArray<Uint64>          SortTexKeys;
        TDynArray<BatchAssignment> Assign;

        // One batch's worth of upload-ready vertices, gathered in sorted order before each draw.
        TFixedArray<QuadVertex, MAX_VERTICES>      SortedBuffer;
        // Current batch's slot -> texture map (slot 0 = white).
        TFixedArray<Texture2D*, MAX_TEXTURE_SLOTS> BatchTextures;

        glm::mat4 ViewProjection = glm::mat4(1.f);
    };
 
    static Renderer2DData s_Data;
    
    // Stats: accumulate the in-flight frame, publish it one frame late (so the overlay's own draws
    // never perturb the numbers it displays). NewFrame() rolls accum -> last.
    static RenderStats s_StatsAccum;
    static RenderStats s_StatsLast;
 
    // =============================================================================
    // Init / Shutdown
    // =============================================================================
 
    void Renderer2D::Init()
    {
        OPAAX_CORE_INFO("Renderer2D::Init()");
 
        // --- VAO + dynamic VBO (sized to one batch — the staging upload unit) ---
        s_Data.QuadVAO = IVertexArray::Create();
 
        auto lVBO = IVertexBuffer::Create(MAX_VERTICES * sizeof(QuadVertex));
        lVBO->SetLayout({
            { EShaderDataType::Float3 },  // Position
            { EShaderDataType::Float4 },  // Color
            { EShaderDataType::Float2 },  // TexCoord
            { EShaderDataType::Float  },  // TexIndex
        });
 
        // Store raw ptr before ownership transfer — needed for SetData on emit
        s_Data.QuadVBO = lVBO.get();
        s_Data.QuadVAO->AddVertexBuffer(Move(lVBO));

        // --- Static index buffer — indices never change for quads ---
        TFixedArray<Uint32, MAX_INDICES> lIndices;
        Uint32 lOffset = 0;
        for (Uint32 i = 0; i < MAX_INDICES; i += 6)
        {
            // Two triangles per quad: 0 1 2  2 3 0
            lIndices[i + 0] = lOffset + 0;
            lIndices[i + 1] = lOffset + 1;
            lIndices[i + 2] = lOffset + 2;
            lIndices[i + 3] = lOffset + 2;
            lIndices[i + 4] = lOffset + 3;
            lIndices[i + 5] = lOffset + 0;
            lOffset += 4;
        }
        s_Data.QuadVAO->SetIndexBuffer(
            IIndexBuffer::Create(lIndices.data(), MAX_INDICES));
 
        // --- White 1x1 texture for solid colour quads (always slot 0) ---
        s_Data.WhiteTexture     = MakeUnique<Texture2D>(1u, 1u);
        s_Data.BatchTextures[0] = s_Data.WhiteTexture.get();
        
        // Persistent record capacity — the frame list grows past MAX_QUADS now; reserve up front so
        // a typical frame never reallocates (CommandCapacity in stats watches this).
        s_Data.Commands.reserve(MAX_QUADS * 4);
 
        // --- Shader ---
        // Batch shader loads from disk (asset pipeline) — direct path ctor, not AssetRegistry:
        // Init runs at RenderSubsystem::Startup, before the loader/manifest are guaranteed ready.
        const OpaaxString lShaderPath = EngineConfig::EngineAssetsRoot() + "/Shaders/Sprite.glsl";
        s_Data.QuadShader = MakeUnique<ShaderAsset>(lShaderPath, OPAAX_ID("Shaders/Sprite"));

        // --- Camera UBO (binding 1) — sole source of u_ViewProjection, written each Begin.
        //     Binding 1 (not 0) so it shares the Vulkan sprite descriptor set with the sampler
        //     array at binding 0; GL is unaffected (separate UBO/texture namespaces).
        s_Data.CameraUBO = IUniformBuffer::Create(static_cast<Uint32>(sizeof(glm::mat4)), 1);

        // --- Sprite pipeline: shader + vertex layout + alpha blend. VertexLayout is consumed by
        //     command-buffer backends (Vulkan); the GL VAO already encodes the layout.
        PipelineDesc lPipelineDesc;
        lPipelineDesc.Shader       = s_Data.QuadShader->GetRHIShader();
        lPipelineDesc.VertexLayout = BufferLayout{
            { EShaderDataType::Float3 },  // Position
            { EShaderDataType::Float4 },  // Color
            { EShaderDataType::Float2 },  // TexCoord
            { EShaderDataType::Float  },  // TexIndex
        };
        lPipelineDesc.Blend     = EBlendMode::Alpha;
        lPipelineDesc.Topology  = EPrimitiveTopology::Triangles;
        lPipelineDesc.DebugName = "Renderer2D::Sprite";
        s_Data.QuadPipeline = IPipeline::Create(lPipelineDesc);

        // --- Bind group: camera UBO (binding 1) + the 16-sampler array. The UBO is set once;
        //     textures are (re)set each emit.
        s_Data.QuadBindGroup = IBindGroup::Create(BindGroupLayout{ 1u, MAX_TEXTURE_SLOTS });
        s_Data.QuadBindGroup->SetUniformBuffer(*s_Data.CameraUBO);
    }
 
    void Renderer2D::Shutdown()
    {
        OPAAX_CORE_INFO("Renderer2D::Shutdown()");
        s_Data.QuadBindGroup.reset();
        s_Data.QuadPipeline.reset();   // before the shader it references
        s_Data.QuadVAO.reset();
        s_Data.QuadShader.reset();
        s_Data.WhiteTexture.reset();
        s_Data.CameraUBO.reset();
    }
    
    // =============================================================================
    // Frame stats
    // =============================================================================

    void Renderer2D::NewFrame()
    {
        s_StatsLast  = s_StatsAccum;
        s_StatsAccum = RenderStats{};
    }

    const RenderStats& Renderer2D::GetStats() { return s_StatsLast; }
 
    // =============================================================================
    // Begin / End
    // =============================================================================
 
    void Renderer2D::Begin(ICamera& InCamera, ICommandBuffer& InCmd)
    {
        s_Data.Cmd            = &InCmd;
        s_Data.ViewProjection = InCamera.GetViewProjection();

        // Bind the sprite pipeline (shader + blend) on the command buffer.
        s_Data.Cmd->BindPipeline(*s_Data.QuadPipeline);

        // u_ViewProjection rides the camera UBO (binding 1) — SPIR-V has no default-block path.
        s_Data.CameraUBO->SetData(glm::value_ptr(s_Data.ViewProjection),
                                  static_cast<Uint32>(sizeof(glm::mat4)));

        StartBatch();
    }

    void Renderer2D::End()
    {
        EmitFrame();
        s_Data.Cmd = nullptr;
    }
 
    void Renderer2D::StartBatch()
    {
        s_Data.Commands.clear();   // keeps capacity — the record is reused frame to frame
    }
    
    // =============================================================================
    // Frame emit — ONE global sort, then batches in sorted order
    // =============================================================================
 
    void Renderer2D::EmitFrame()
    {
        const Uint32 lCount = static_cast<Uint32>(s_Data.Commands.size());
        if (lCount == 0) { return; }

        // --- Frame-global stable sort by draw key. Stable => equal keys keep submission order.
        //     Painter's algorithm: ascending key draws back-to-front; depth test stays OFF (correct
        //     for alpha-blended 2D). The key is (Layer, OrderInLayer) only — texture grouping is the
        //     per-batch slot window's job, so same-band overlapping sprites keep submission order. ---
        s_Data.SortIndices.resize(lCount);
        for (Uint32 i = 0; i < lCount; ++i) { s_Data.SortIndices[i] = i; }

        const auto lSortStart = std::chrono::steady_clock::now();
        std::stable_sort(s_Data.SortIndices.begin(), s_Data.SortIndices.end(),
            [](Uint32 InA, Uint32 InB)
            { return s_Data.Commands[InA].SortKey < s_Data.Commands[InB].SortKey; });
        const auto lSortEnd = std::chrono::steady_clock::now();
        s_StatsAccum.SortMicros +=
            std::chrono::duration<double, std::micro>(lSortEnd - lSortStart).count();

        // --- Texture identities in sorted order -> pure batch/slot assignment ---
        s_Data.SortTexKeys.resize(lCount);
        s_Data.Assign.resize(lCount);
        for (Uint32 k = 0; k < lCount; ++k)
        {
            s_Data.SortTexKeys[k] =
                 reinterpret_cast<Uint64>(s_Data.Commands[s_Data.SortIndices[k]].Texture);
        }
        AssignBatches(s_Data.SortTexKeys.data(), lCount, MAX_QUADS, MAX_TEXTURE_SLOTS,
                      s_Data.Assign.data());
        
        // --- Walk sorted commands; gather each batch into the staging buffer; emit on boundaries ---
        Uint32 lCurrentBatch = 0;
        Uint32 lQuadInBatch  = 0;
        Uint32 lSlotCount    = 1;                          // slot 0 = white
        s_Data.BatchTextures[0] = s_Data.WhiteTexture.get();

        for (Uint32 k = 0; k < lCount; ++k)
        {
            const BatchAssignment& lBA  = s_Data.Assign[k];
            const QuadCommand&     lCmd = s_Data.Commands[s_Data.SortIndices[k]];

            if (lBA.BatchIndex != lCurrentBatch)
            {
                EmitBatch(lQuadInBatch, lSlotCount);
                lCurrentBatch = lBA.BatchIndex;
                lQuadInBatch  = 0;
                lSlotCount    = 1;
                s_Data.BatchTextures[0] = s_Data.WhiteTexture.get();
            }

            if (lBA.Slot != 0)
            {
                s_Data.BatchTextures[lBA.Slot] = lCmd.Texture;
                if (lBA.Slot + 1 > lSlotCount) { lSlotCount = lBA.Slot + 1; }
        }

            // Copy the 4 vertices into the staging buffer, stamping the resolved slot as TexIndex.
            const Uint32 lDst = lQuadInBatch * 4;
            for (Uint32 v = 0; v < 4; ++v)
            {
                QuadVertex lVert = lCmd.Vertices[v];
                lVert.TexIndex   = static_cast<float>(lBA.Slot);
                s_Data.SortedBuffer[lDst + v] = lVert;
            }
            ++lQuadInBatch;
        }
        EmitBatch(lQuadInBatch, lSlotCount);               // final partial batch

        s_StatsAccum.CommandCapacity = static_cast<Uint32>(s_Data.Commands.capacity());
    }

    void Renderer2D::EmitBatch(Uint32 InQuadCount, Uint32 InSlotCount)
    {
        if (InQuadCount == 0) { return; }

        const Uint32 lDataSize = InQuadCount * 4u * static_cast<Uint32>(sizeof(QuadVertex));
        s_Data.QuadVBO->SetData(s_Data.SortedBuffer.data(), lDataSize);

        // Every sampler unit must reference a live texture (no dangling descriptor across draws):
        // active slots get their texture, the rest get white.
        for (Uint32 i = 0; i < MAX_TEXTURE_SLOTS; ++i)
        {
            Texture2D* lTex = (i < InSlotCount) ? s_Data.BatchTextures[i] : s_Data.WhiteTexture.get();
            if (!lTex) { lTex = s_Data.WhiteTexture.get(); }
            s_Data.QuadBindGroup->SetTexture(i, *lTex->GetRHITexture());
        }

        s_Data.Cmd->BindBindGroup(*s_Data.QuadBindGroup);
        s_Data.Cmd->BindVertexArray(*s_Data.QuadVAO);
        s_Data.Cmd->DrawIndexed(InQuadCount * 6);

        ++s_StatsAccum.Batches;
        ++s_StatsAccum.DrawCalls;
        s_StatsAccum.Quads += InQuadCount;
        if (InSlotCount > s_StatsAccum.PeakTextureSlots) { s_StatsAccum.PeakTextureSlots = InSlotCount; }
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
        
        // Computes the four corners (BL, BR, TR, TL) of a quad, skipping trig when un-rotated.
        FORCEINLINE void ComputeCorners(const Vector2F& InPosition, float InHalfW, float InHalfH,
                                        float InRotationRad,
                                        Vector2F& OutBL, Vector2F& OutBR, Vector2F& OutTR, Vector2F& OutTL)
        {
            if (InRotationRad == 0.f)
            {
                OutBL = { InPosition.x - InHalfW, InPosition.y - InHalfH };
                OutBR = { InPosition.x + InHalfW, InPosition.y - InHalfH };
                OutTR = { InPosition.x + InHalfW, InPosition.y + InHalfH };
                OutTL = { InPosition.x - InHalfW, InPosition.y + InHalfH };
            }
            else
            {
                const float lCos = std::cos(InRotationRad);
                const float lSin = std::sin(InRotationRad);
                OutBL = RotateOffset(InPosition, lCos, lSin, -InHalfW, -InHalfH);
                OutBR = RotateOffset(InPosition, lCos, lSin, +InHalfW, -InHalfH);
                OutTR = RotateOffset(InPosition, lCos, lSin, +InHalfW, +InHalfH);
                OutTL = RotateOffset(InPosition, lCos, lSin, -InHalfW, +InHalfH);
            }
        }

        // MakeSortKey hoisted to Renderer/Renderer2DSortKey.h (unit-tested in isolation).
    }

    void Renderer2D::DrawQuad(  const Vector2F& InPosition,
                                const Vector2F& InSize,
                                const Vector4F& InColor,
                                float           InRotationRad,
                                ERenderLayer    InLayer,
                                Int16           InOrderInLayer)
    {
        Vector2F lBL, lBR, lTR, lTL;
        ComputeCorners(InPosition, InSize.x * 0.5f, InSize.y * 0.5f, InRotationRad, lBL, lBR, lTR, lTL);
        QuadCommand lCmd;
        lCmd.SortKey = MakeSortKey(InLayer, InOrderInLayer, 0u);
        lCmd.Texture = nullptr;                       // white (slot 0), resolved at emit

        constexpr float lTexIndex = 0.f;              // placeholder — EmitFrame stamps the real slot
        lCmd.Vertices[0] = { { lBL.x, lBL.y, 0.f }, InColor, { 0.f, 0.f }, lTexIndex };
        lCmd.Vertices[1] = { { lBR.x, lBR.y, 0.f }, InColor, { 1.f, 0.f }, lTexIndex };
        lCmd.Vertices[2] = { { lTR.x, lTR.y, 0.f }, InColor, { 1.f, 1.f }, lTexIndex };
        lCmd.Vertices[3] = { { lTL.x, lTL.y, 0.f }, InColor, { 0.f, 1.f }, lTexIndex };

        s_Data.Commands.push_back(lCmd);
    }

    void Renderer2D::DrawSprite(const Vector2F& InPosition, const Vector2F& InSize, const TextureHandle& InTexture,
        const Vector4F& InColor, float InRotationRad, ERenderLayer InLayer, Int16 InOrderInLayer)
    {
        OPAAX_CORE_ASSERT(InTexture.IsValid())
        DrawSprite(InPosition, InSize, *InTexture.Get(), InColor, InRotationRad, InLayer, InOrderInLayer);
    }

    void Renderer2D::DrawSprite(const Vector2F& InPosition, const Vector2F& InSize, const TextureHandle& InTexture,
        const Vector2F& InUVMin, const Vector2F& InUVMax, const Vector4F& InColor, float InRotationRad,
        ERenderLayer InLayer, Int16 InOrderInLayer)
    {
        OPAAX_CORE_ASSERT(InTexture.IsValid())
        DrawSprite(InPosition, InSize, *InTexture.Get(), InUVMin, InUVMax, InColor, InRotationRad, InLayer, InOrderInLayer);
    }

    void Renderer2D::DrawSprite(const Vector2F& InPosition,
                                const Vector2F& InSize,
                                Texture2D&      InTexture,
                                const Vector4F& InColor,
                                float           InRotationRad,
                                ERenderLayer    InLayer,
                                Int16           InOrderInLayer)
    {
        DrawSprite(InPosition, InSize, InTexture,
                   { 0.f, 0.f }, { 1.f, 1.f }, InColor, InRotationRad, InLayer, InOrderInLayer);
    }

    void Renderer2D::DrawSprite(const Vector2F& InPosition,
                                const Vector2F& InSize,
                                Texture2D&      InTexture,
                                const Vector2F& InUVMin,
                                const Vector2F& InUVMax,
                                const Vector4F& InColor,
                                float           InRotationRad,
                                ERenderLayer    InLayer,
                                Int16           InOrderInLayer)
    {
        Vector2F lBL, lBR, lTR, lTL;
        ComputeCorners(InPosition, InSize.x * 0.5f, InSize.y * 0.5f, InRotationRad, lBL, lBR, lTR, lTL);

        QuadCommand lCmd;
        lCmd.SortKey = MakeSortKey(InLayer, InOrderInLayer, 0u);  // slot resolved per-batch at emit
        lCmd.Texture = &InTexture;

        constexpr float lTexIndex = 0.f;                         // placeholder — EmitFrame stamps the slot
        lCmd.Vertices[0] = { { lBL.x, lBL.y, 0.f }, InColor, { InUVMin.x, InUVMin.y }, lTexIndex };
        lCmd.Vertices[1] = { { lBR.x, lBR.y, 0.f }, InColor, { InUVMax.x, InUVMin.y }, lTexIndex };
        lCmd.Vertices[2] = { { lTR.x, lTR.y, 0.f }, InColor, { InUVMax.x, InUVMax.y }, lTexIndex };
        lCmd.Vertices[3] = { { lTL.x, lTL.y, 0.f }, InColor, { InUVMin.x, InUVMax.y }, lTexIndex };

        s_Data.Commands.push_back(lCmd);
    }

} // namespace Opaax