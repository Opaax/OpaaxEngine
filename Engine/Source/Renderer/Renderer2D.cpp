#include "Renderer2D.h"

#include "RHI/IRHIDevice.h"
#include "RHI/Buffer.h"
#include "RHI/Texture.h"
#include "RHI/Shader.h"
#include "RHI/UniformBuffer.h"
#include "RHI/Pipeline.h"
#include "RHI/BindGroup.h"
#include "RHI/ICommandBuffer.h"
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
    // Renderer2DData — the pImpl. All GPU + batch state, owned by one Renderer2D instance.
    //   GPU resources are raw RHI types (ITexture2D/IShader/...), created either through the
    //   device (live path) or the global factories (transitional path) — the members are the same.
    // =============================================================================
    struct Renderer2DData
    {
        UniquePtr<IVertexArray>   QuadVAO;
        IVertexBuffer*            QuadVBO      = nullptr;  // non-owning, owned by VAO
        UniquePtr<IShader>        QuadShader;
        UniquePtr<ITexture2D>     WhiteTexture;
        UniquePtr<IUniformBuffer> CameraUBO;  // binding 1: u_ViewProjection (std140)
        UniquePtr<IPipeline>      QuadPipeline;     // sprite pipeline (shader + layout + alpha blend)
        UniquePtr<IBindGroup>     QuadBindGroup;    // camera UBO + 16-sampler array
        ICommandBuffer*           Cmd          = nullptr;  // active recorder, set in Begin (non-owning)

        // CPU-side vertex buffer — filled each frame, uploaded on flush
        TFixedArray<QuadVertex, MAX_VERTICES> VertexBuffer;
        QuadVertex*                           VertexBufferPtr = nullptr;  // write cursor
        Uint32                                QuadCount       = 0;

        // Per-quad draw-order key (parallel to the quads in VertexBuffer), and a scratch
        // buffer the flush gathers vertices into in sorted order before upload.
        TFixedArray<Uint64,     MAX_QUADS>    SortKeys;
        TFixedArray<QuadVertex, MAX_VERTICES> SortedBuffer;

        // Texture slot tracking
        TFixedArray<ITexture2D*, MAX_TEXTURE_SLOTS> TextureSlots;
        Uint32                                      TextureSlotIndex = 1; // slot 0 = white

        glm::mat4 ViewProjection = glm::mat4(1.f);
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
            };
        }

        void FillQuadIndices(TFixedArray<Uint32, MAX_INDICES>& OutIndices)
        {
            Uint32 lOffset = 0;
            for (Uint32 i = 0; i < MAX_INDICES; i += 6)
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
    }

    // =============================================================================
    // Init (live path) — everything through the device; no global factory / GetBackend.
    // NOTE: InLimits is accepted for the contract but the batch buffers are compile-time sized
    //   (MAX_QUADS/MAX_TEXTURE_SLOTS); honoring runtime limits needs dynamic buffers (deferred).
    // =============================================================================
    void Renderer2D::Init(IRHIDevice& InDevice, const RenderLimits& /*InLimits*/, const ShaderDesc& InShader)
    {
        OPAAX_LOG(LogRenderer2D, Info, "Renderer2D::Init(device)")

        m_Data->QuadVAO = InDevice.CreateVertexArray();

        UniquePtr<IVertexBuffer> lVBO = InDevice.CreateVertexBuffer(MAX_VERTICES * sizeof(QuadVertex));
        lVBO->SetLayout(MakeQuadLayout());
        m_Data->QuadVBO = lVBO.get();
        m_Data->QuadVAO->AddVertexBuffer(Move(lVBO));

        TFixedArray<Uint32, MAX_INDICES> lIndices;
        FillQuadIndices(lIndices);
        m_Data->QuadVAO->SetIndexBuffer(InDevice.CreateIndexBuffer(lIndices.data(), MAX_INDICES));

        m_Data->WhiteTexture    = InDevice.CreateTexture(1u, 1u);
        m_Data->TextureSlots[0] = m_Data->WhiteTexture.get();

        m_Data->QuadShader   = InDevice.CreateShader(InShader);
        m_Data->CameraUBO    = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(glm::mat4)), 1);
        m_Data->QuadPipeline = InDevice.CreatePipeline(MakeSpritePipelineDesc(m_Data->QuadShader.get()));

        m_Data->QuadBindGroup = InDevice.CreateBindGroup(BindGroupLayout{ 1u, MAX_TEXTURE_SLOTS });
        m_Data->QuadBindGroup->SetUniformBuffer(*m_Data->CameraUBO);
    }

    void Renderer2D::Shutdown()
    {
        if (!m_Data) { return; }

        OPAAX_LOG(LogRenderer2D, Info, "Renderer2D::Shutdown()")
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
    void Renderer2D::BeginInternal(const Matrix44F& InViewProjection, ICommandBuffer& InCmd)
    {
        m_Data->Cmd            = &InCmd;
        m_Data->ViewProjection = InViewProjection;

        // Bind the sprite pipeline (shader + blend); upload the view-projection to the camera UBO.
        m_Data->Cmd->BindPipeline(*m_Data->QuadPipeline);
        m_Data->CameraUBO->SetData(glm::value_ptr(m_Data->ViewProjection),
                                   static_cast<Uint32>(sizeof(glm::mat4)));
        StartBatch();
    }

    void Renderer2D::BeginScene(const RenderView& InView, ICommandBuffer& InCmd)
    {
        InCmd.SetViewport(InView.Viewport.X, InView.Viewport.Y, InView.Viewport.Width, InView.Viewport.Height);
        BeginInternal(InView.ViewProjection, InCmd);
    }

    void Renderer2D::End()
    {
        Flush();
        m_Data->Cmd = nullptr;
    }

    void Renderer2D::StartBatch()
    {
        m_Data->QuadCount        = 0;
        m_Data->VertexBufferPtr  = m_Data->VertexBuffer.data();
        m_Data->TextureSlotIndex = 1;  // slot 0 = white, always bound
    }

    void Renderer2D::Flush()
    {
        if (m_Data->QuadCount == 0) { return; }

        // Sort the quad draw order by (Layer, OrderInLayer, textureSlot). Stable so equal keys
        // keep submission order. Painter's algorithm — ascending key draws back-to-front; depth
        // test stays OFF (correct for alpha-blended 2D). Orders the CURRENT batch only.
        TFixedArray<Uint32, MAX_QUADS> lOrder;
        for (Uint32 i = 0; i < m_Data->QuadCount; ++i) { lOrder[i] = i; }

        std::stable_sort(lOrder.data(), lOrder.data() + m_Data->QuadCount,
            [this](Uint32 InA, Uint32 InB) { return m_Data->SortKeys[InA] < m_Data->SortKeys[InB]; });

        for (Uint32 i = 0; i < m_Data->QuadCount; ++i)
        {
            const Uint32 lSrc = lOrder[i] * 4;
            const Uint32 lDst = i * 4;
            m_Data->SortedBuffer[lDst + 0] = m_Data->VertexBuffer[lSrc + 0];
            m_Data->SortedBuffer[lDst + 1] = m_Data->VertexBuffer[lSrc + 1];
            m_Data->SortedBuffer[lDst + 2] = m_Data->VertexBuffer[lSrc + 2];
            m_Data->SortedBuffer[lDst + 3] = m_Data->VertexBuffer[lSrc + 3];
        }

        const Uint32 lDataSize = m_Data->QuadCount * 4u * static_cast<Uint32>(sizeof(QuadVertex));
        m_Data->QuadVBO->SetData(m_Data->SortedBuffer.data(), lDataSize);

        // Populate the sampler array. Active slots get their texture; inactive slots get the
        // white texture so every unit references a live texture (no dangling across flushes).
        for (Uint32 i = 0; i < MAX_TEXTURE_SLOTS; ++i)
        {
            ITexture2D* lTex = (i < m_Data->TextureSlotIndex) ? m_Data->TextureSlots[i]
                                                              : m_Data->WhiteTexture.get();
            m_Data->QuadBindGroup->SetTexture(i, *lTex);
        }

        m_Data->Cmd->BindBindGroup(*m_Data->QuadBindGroup);
        m_Data->Cmd->BindVertexArray(*m_Data->QuadVAO);
        m_Data->Cmd->DrawIndexed(m_Data->QuadCount * 6);
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
        if (m_Data->QuadCount >= MAX_QUADS)
        {
            Flush();
            StartBatch();
        }

        constexpr float lTexIndex = 0.f;  // white texture (slot 0)

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

        // Bottom-left
        m_Data->VertexBufferPtr->Position = { lBL.x, lBL.y, 0.f };
        m_Data->VertexBufferPtr->Color    = InColor;
        m_Data->VertexBufferPtr->TexCoord = { 0.f, 0.f };
        m_Data->VertexBufferPtr->TexIndex = lTexIndex;
        ++m_Data->VertexBufferPtr;

        // Bottom-right
        m_Data->VertexBufferPtr->Position = { lBR.x, lBR.y, 0.f };
        m_Data->VertexBufferPtr->Color    = InColor;
        m_Data->VertexBufferPtr->TexCoord = { 1.f, 0.f };
        m_Data->VertexBufferPtr->TexIndex = lTexIndex;
        ++m_Data->VertexBufferPtr;

        // Top-right
        m_Data->VertexBufferPtr->Position = { lTR.x, lTR.y, 0.f };
        m_Data->VertexBufferPtr->Color    = InColor;
        m_Data->VertexBufferPtr->TexCoord = { 1.f, 1.f };
        m_Data->VertexBufferPtr->TexIndex = lTexIndex;
        ++m_Data->VertexBufferPtr;

        // Top-left
        m_Data->VertexBufferPtr->Position = { lTL.x, lTL.y, 0.f };
        m_Data->VertexBufferPtr->Color    = InColor;
        m_Data->VertexBufferPtr->TexCoord = { 0.f, 1.f };
        m_Data->VertexBufferPtr->TexIndex = lTexIndex;
        ++m_Data->VertexBufferPtr;

        m_Data->SortKeys[m_Data->QuadCount] = MakeSortKey(InLayer, InOrderInLayer, 0u);
        ++m_Data->QuadCount;
    }

} // namespace Opaax
