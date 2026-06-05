#include "Renderer2D.h"
#include "RHI/RenderCommand.h"
#include "RHI/Buffer.h"
#include "RHI/UniformBuffer.h"
#include "RHI/Pipeline.h"
#include "RHI/BindGroup.h"
#include "RHI/ICommandBuffer.h"
#include "Renderer/ShaderAsset.h"
#include "Renderer/Texture2D.h"
#include "Renderer/Camera/ICamera.h"
#include "Core/Config/EngineConfig.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/EngineAPI.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace Opaax
{
    // =============================================================================
    // Batch constants
    // =============================================================================
    static constexpr Uint32 MAX_QUADS         = 1000;
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
        float     TexIndex;     // texture slot index (float for shader compatibility)
    };
 
    // =============================================================================
    // Renderer2D internal state
    // =============================================================================
    struct Renderer2DData
    {
        UniquePtr<IVertexArray>  QuadVAO;
        IVertexBuffer*           QuadVBO      = nullptr;  // non-owning, owned by VAO
        UniquePtr<ShaderAsset>   QuadShader;
        UniquePtr<Texture2D>     WhiteTexture;
        UniquePtr<IUniformBuffer> CameraUBO;  // binding 0: u_ViewProjection (std140)
        UniquePtr<IPipeline>     QuadPipeline;     // sprite pipeline (shader + layout + alpha blend)
        UniquePtr<IBindGroup>    QuadBindGroup;    // camera UBO + 16-sampler array
        ICommandBuffer*          Cmd          = nullptr;  // active recorder, set in Begin (non-owning)
 
        // CPU-side vertex buffer — filled each frame, uploaded on flush
        TFixedArray<QuadVertex, MAX_VERTICES> VertexBuffer;
        QuadVertex*                           VertexBufferPtr = nullptr;  // write cursor
        Uint32                                QuadCount       = 0;

        // Per-quad draw-order key (parallel to the quads in VertexBuffer), and a scratch
        // buffer the flush gathers vertices into in sorted order before upload.
        TFixedArray<Uint64,     MAX_QUADS>    SortKeys;
        TFixedArray<QuadVertex, MAX_VERTICES> SortedBuffer;

        // Texture slot tracking
        TFixedArray<Texture2D*, MAX_TEXTURE_SLOTS> TextureSlots;
        Uint32                                          TextureSlotIndex = 1; // slot 0 = white

        glm::mat4 ViewProjection = glm::mat4(1.f);
    };
 
    static Renderer2DData s_Data;
 
    // =============================================================================
    // Init / Shutdown
    // =============================================================================
 
    void Renderer2D::Init()
    {
        OPAAX_CORE_INFO("Renderer2D::Init()");
 
        // --- VAO + dynamic VBO ---
        s_Data.QuadVAO = IVertexArray::Create();
 
        auto lVBO = IVertexBuffer::Create(MAX_VERTICES * sizeof(QuadVertex));
        lVBO->SetLayout({
            { EShaderDataType::Float3 },  // Position
            { EShaderDataType::Float4 },  // Color
            { EShaderDataType::Float2 },  // TexCoord
            { EShaderDataType::Float  },  // TexIndex
        });
 
        // Store raw ptr before ownership transfer — needed for SetData on flush
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
 
        // --- White 1x1 texture for solid colour quads ---
        s_Data.WhiteTexture  = MakeUnique<Texture2D>(1u, 1u);
        s_Data.TextureSlots[0] = s_Data.WhiteTexture.get();
 
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

        // --- Bind group: camera UBO (binding 0) + the 16-sampler array. The UBO is set once;
        //     textures are (re)set each flush.
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
    // Begin / End
    // =============================================================================
 
    void Renderer2D::Begin(ICamera& InCamera, ICommandBuffer& InCmd)
    {
        s_Data.Cmd            = &InCmd;
        s_Data.ViewProjection = InCamera.GetViewProjection();

        // Bind the sprite pipeline (shader + blend) on the command buffer.
        s_Data.Cmd->BindPipeline(*s_Data.QuadPipeline);

        // u_ViewProjection rides the camera UBO (binding 0) — SPIR-V has no default-block path.
        s_Data.CameraUBO->SetData(glm::value_ptr(s_Data.ViewProjection),
                                  static_cast<Uint32>(sizeof(glm::mat4)));

        StartBatch();
    }

    void Renderer2D::End()
    {
        Flush();
        s_Data.Cmd = nullptr;
    }
 
    void Renderer2D::StartBatch()
    {
        s_Data.QuadCount        = 0;
        s_Data.VertexBufferPtr  = s_Data.VertexBuffer.data();
        s_Data.TextureSlotIndex = 1;  // slot 0 = white, always bound
    }
 
    void Renderer2D::Flush()
    {
        if (s_Data.QuadCount == 0) { return; }

        // Sort the quad draw order by (Layer, OrderInLayer, textureSlot). Stable so equal
        // keys keep submission order. Painter's algorithm — ascending key draws back-to-front;
        // depth test stays OFF (correct for alpha-blended 2D).
        //
        // NOTE: this orders the CURRENT batch only. A frame exceeding MAX_QUADS or
        //   MAX_TEXTURE_SLOTS splits into multiple flushes, ordered by batch emission. For the
        //   target 2D games one batch is the norm, so layering is fully resolved per frame. A
        //   global sort would need deferred flushing + sorted-emit texture assignment.
        TFixedArray<Uint32, MAX_QUADS> lOrder;
        for (Uint32 i = 0; i < s_Data.QuadCount; ++i) { lOrder[i] = i; }

        std::stable_sort(lOrder.data(), lOrder.data() + s_Data.QuadCount,
            [](Uint32 InA, Uint32 InB) { return s_Data.SortKeys[InA] < s_Data.SortKeys[InB]; });

        // Gather each quad's 4 vertices into the upload buffer in sorted order. TexIndex values
        // stay valid — reordering quads never changes which slot a texture lives in.
        for (Uint32 i = 0; i < s_Data.QuadCount; ++i)
        {
            const Uint32 lSrc = lOrder[i] * 4;
            const Uint32 lDst = i * 4;
            s_Data.SortedBuffer[lDst + 0] = s_Data.VertexBuffer[lSrc + 0];
            s_Data.SortedBuffer[lDst + 1] = s_Data.VertexBuffer[lSrc + 1];
            s_Data.SortedBuffer[lDst + 2] = s_Data.VertexBuffer[lSrc + 2];
            s_Data.SortedBuffer[lDst + 3] = s_Data.VertexBuffer[lSrc + 3];
        }

        const Uint32 lDataSize = s_Data.QuadCount * 4u * static_cast<Uint32>(sizeof(QuadVertex));
        s_Data.QuadVBO->SetData(s_Data.SortedBuffer.data(), lDataSize);

        // Populate the bind group's sampler array. Active slots get their texture; inactive slots
        // get the white texture so every unit references a live texture (no dangling across flushes).
        for (Uint32 i = 0; i < MAX_TEXTURE_SLOTS; ++i)
        {
            Texture2D* lTex = (i < s_Data.TextureSlotIndex) ? s_Data.TextureSlots[i]
                                                            : s_Data.WhiteTexture.get();
            s_Data.QuadBindGroup->SetTexture(i, *lTex->GetRHITexture());
        }

        s_Data.Cmd->BindBindGroup(*s_Data.QuadBindGroup);
        s_Data.Cmd->BindVertexArray(*s_Data.QuadVAO);
        s_Data.Cmd->DrawIndexed(s_Data.QuadCount * 6);
    }
 
    // =============================================================================
    // Texture slot resolution
    //
    // Returns the slot index for a given texture.
    // If the texture is not already in a slot, assigns the next free one.
    // If all slots are full, flushes first to start a new batch.
    // =============================================================================
    float Renderer2D::GetTextureSlot(Texture2D& InTexture)
    {
        // Search existing slots
        for (Uint32 i = 1; i < s_Data.TextureSlotIndex; ++i)
        {
            if (s_Data.TextureSlots[i] == &InTexture)
            {
                return static_cast<float>(i);
            }
        }
 
        // All slots full — flush and start a new batch
        if (s_Data.TextureSlotIndex >= MAX_TEXTURE_SLOTS)
        {
            Flush();
            StartBatch();
        }
 
        const float lSlot = static_cast<float>(s_Data.TextureSlotIndex);
        s_Data.TextureSlots[s_Data.TextureSlotIndex] = &InTexture;
        ++s_Data.TextureSlotIndex;
        return lSlot;
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

        // Pack draw order into one sortable key: [Layer:hi][OrderInLayer:mid][texSlot:lo].
        // OrderInLayer (Int16) is biased to unsigned so negative orders sort before positive.
        FORCEINLINE Uint64 MakeSortKey(ERenderLayer InLayer, Int16 InOrderInLayer, Uint32 InTexSlot)
        {
            const Uint64 lLayer = static_cast<Uint64>(static_cast<Uint8>(InLayer));
            const Uint64 lOrder = static_cast<Uint64>(static_cast<Int32>(InOrderInLayer) + 32768); // [0, 65535]
            const Uint64 lSlot  = static_cast<Uint64>(InTexSlot & 0xFFu);
            return (lLayer << 32) | (lOrder << 8) | lSlot;
        }
    }

    void Renderer2D::DrawQuad(const Vector2F& InPosition,
                               const Vector2F& InSize,
                               const Vector4F& InColor,
                               float           InRotationRad,
                               ERenderLayer    InLayer,
                               Int16           InOrderInLayer)
    {
        if (s_Data.QuadCount >= MAX_QUADS)
        {
            Flush();
            StartBatch();
        }

        constexpr float lTexIndex = 0.f;  // white texture (slot 0)

        const float lHalfW = InSize.x * 0.5f;
        const float lHalfH = InSize.y * 0.5f;

        // Compute corners (BL, BR, TR, TL). Skip trig when un-rotated.
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
            const float lCos = std::cos(InRotationRad);
            const float lSin = std::sin(InRotationRad);
            lBL = RotateOffset(InPosition, lCos, lSin, -lHalfW, -lHalfH);
            lBR = RotateOffset(InPosition, lCos, lSin, +lHalfW, -lHalfH);
            lTR = RotateOffset(InPosition, lCos, lSin, +lHalfW, +lHalfH);
            lTL = RotateOffset(InPosition, lCos, lSin, -lHalfW, +lHalfH);
        }

        // Bottom-left
        s_Data.VertexBufferPtr->Position = { lBL.x, lBL.y, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { 0.f, 0.f };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;

        // Bottom-right
        s_Data.VertexBufferPtr->Position = { lBR.x, lBR.y, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { 1.f, 0.f };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;

        // Top-right
        s_Data.VertexBufferPtr->Position = { lTR.x, lTR.y, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { 1.f, 1.f };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;

        // Top-left
        s_Data.VertexBufferPtr->Position = { lTL.x, lTL.y, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { 0.f, 1.f };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;

        s_Data.SortKeys[s_Data.QuadCount] = MakeSortKey(InLayer, InOrderInLayer, 0u);
        ++s_Data.QuadCount;
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
        if (s_Data.QuadCount >= MAX_QUADS)
        {
            Flush();
            StartBatch();
        }

        const float lTexIndex = GetTextureSlot(InTexture);
        const float lHalfW    = InSize.x * 0.5f;
        const float lHalfH    = InSize.y * 0.5f;

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
            const float lCos = std::cos(InRotationRad);
            const float lSin = std::sin(InRotationRad);
            lBL = RotateOffset(InPosition, lCos, lSin, -lHalfW, -lHalfH);
            lBR = RotateOffset(InPosition, lCos, lSin, +lHalfW, -lHalfH);
            lTR = RotateOffset(InPosition, lCos, lSin, +lHalfW, +lHalfH);
            lTL = RotateOffset(InPosition, lCos, lSin, -lHalfW, +lHalfH);
        }

        // Bottom-left
        s_Data.VertexBufferPtr->Position = { lBL.x, lBL.y, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { InUVMin.x, InUVMin.y };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;

        // Bottom-right
        s_Data.VertexBufferPtr->Position = { lBR.x, lBR.y, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { InUVMax.x, InUVMin.y };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;

        // Top-right
        s_Data.VertexBufferPtr->Position = { lTR.x, lTR.y, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { InUVMax.x, InUVMax.y };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;

        // Top-left
        s_Data.VertexBufferPtr->Position = { lTL.x, lTL.y, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { InUVMin.x, InUVMax.y };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;

        s_Data.SortKeys[s_Data.QuadCount] = MakeSortKey(InLayer, InOrderInLayer, static_cast<Uint32>(lTexIndex));
        ++s_Data.QuadCount;
    }

} // namespace Opaax