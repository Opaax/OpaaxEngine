#include "Renderer2D.h"
#include "RHI/RenderCommand.h"
#include "RHI/OpenGL/OpenGLBuffer.h"
#include "RHI/OpenGL/OpenGLVertexArray.h"
#include "RHI/OpenGL/OpenGLShader.h"
#include "RHI/OpenGL/OpenGLTexture2D.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/EngineAPI.h"
 
#include <glm/gtc/matrix_transform.hpp>
#include <array>
 
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
        UniquePtr<OpenGLShader>  QuadShader;
        UniquePtr<OpenGLTexture2D> WhiteTexture;
 
        // CPU-side vertex buffer — filled each frame, uploaded on flush
        std::array<QuadVertex, MAX_VERTICES> VertexBuffer;
        QuadVertex*                          VertexBufferPtr = nullptr;  // write cursor
        Uint32                               QuadCount       = 0;
 
        // Texture slot tracking
        std::array<OpenGLTexture2D*, MAX_TEXTURE_SLOTS> TextureSlots;
        Uint32                                          TextureSlotIndex = 1; // slot 0 = white
 
        glm::mat4 ViewProjection = glm::mat4(1.f);
    };
 
    static Renderer2DData s_Data;
 
    // =============================================================================
    // GLSL source — inline, no file I/O (asset system is Milestone 4)
    // =============================================================================
    static constexpr const char* s_VertexShaderSrc = R"(
        #version 450 core
 
        layout(location = 0) in vec3  a_Position;
        layout(location = 1) in vec4  a_Color;
        layout(location = 2) in vec2  a_TexCoord;
        layout(location = 3) in float a_TexIndex;
 
        uniform mat4 u_ViewProjection;
 
        out vec4  v_Color;
        out vec2  v_TexCoord;
        out float v_TexIndex;
 
        void main()
        {
            gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
            v_Color     = a_Color;
            v_TexCoord  = a_TexCoord;
            v_TexIndex  = a_TexIndex;
        }
    )";
 
    static constexpr const char* s_FragmentShaderSrc = R"(
        #version 450 core
 
        in vec4  v_Color;
        in vec2  v_TexCoord;
        in float v_TexIndex;
 
        uniform sampler2D u_Textures[16];
 
        out vec4 FragColor;
 
        void main()
        {
            int   lIdx    = int(v_TexIndex);
            vec4  lSample = texture(u_Textures[lIdx], v_TexCoord);
            FragColor     = lSample * v_Color;
        }
    )";
 
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
        s_Data.QuadVAO->AddVertexBuffer(std::move(lVBO));
 
        // --- Static index buffer — indices never change for quads ---
        std::array<Uint32, MAX_INDICES> lIndices;
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
        s_Data.WhiteTexture  = MakeUnique<OpenGLTexture2D>(1, 1);
        s_Data.TextureSlots[0] = s_Data.WhiteTexture.get();
 
        // --- Shader ---
        s_Data.QuadShader = MakeUnique<OpenGLShader>(s_VertexShaderSrc, s_FragmentShaderSrc);
        s_Data.QuadShader->Bind();
 
        // Bind sampler uniforms once — slots don't change
        Int32 lSamplers[MAX_TEXTURE_SLOTS];
        for (Int32 i = 0; i < static_cast<Int32>(MAX_TEXTURE_SLOTS); ++i) { lSamplers[i] = i; }
        s_Data.QuadShader->SetIntArray("u_Textures", lSamplers, MAX_TEXTURE_SLOTS);
    }
 
    void Renderer2D::Shutdown()
    {
        OPAAX_CORE_INFO("Renderer2D::Shutdown()");
        s_Data.QuadVAO.reset();
        s_Data.QuadShader.reset();
        s_Data.WhiteTexture.reset();
    }
 
    // =============================================================================
    // Begin / End
    // =============================================================================
 
    void Renderer2D::Begin(Camera2D& InCamera)
    {
        s_Data.ViewProjection = InCamera.GetViewProjection();
 
        s_Data.QuadShader->Bind();
        s_Data.QuadShader->SetMat4("u_ViewProjection", s_Data.ViewProjection);
 
        StartBatch();
    }
 
    void Renderer2D::End()
    {
        Flush();
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
 
        // Upload only the vertices we actually wrote
        const Uint32 lDataSize = static_cast<Uint32>(
            reinterpret_cast<uint8_t*>(s_Data.VertexBufferPtr) -
            reinterpret_cast<uint8_t*>(s_Data.VertexBuffer.data()));
 
        s_Data.QuadVBO->SetData(s_Data.VertexBuffer.data(), lDataSize);
 
        // Bind all active texture slots
        for (Uint32 i = 0; i < s_Data.TextureSlotIndex; ++i)
        {
            s_Data.TextureSlots[i]->Bind(i);
        }
 
        s_Data.QuadVAO->Bind();
        RenderCommand::DrawIndexed(s_Data.QuadCount * 6);
    }
 
    // =============================================================================
    // Texture slot resolution
    //
    // Returns the slot index for a given texture.
    // If the texture is not already in a slot, assigns the next free one.
    // If all slots are full, flushes first to start a new batch.
    // =============================================================================
    float Renderer2D::GetTextureSlot(OpenGLTexture2D& InTexture)
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
 
    void Renderer2D::DrawQuad(const Vector2F& InPosition,
                               const Vector2F& InSize,
                               const Vector4F& InColor)
    {
        if (s_Data.QuadCount >= MAX_QUADS)
        {
            Flush();
            StartBatch();
        }
 
        constexpr float lTexIndex = 0.f;  // white texture
 
        const float lHalfW = InSize.x * 0.5f;
        const float lHalfH = InSize.y * 0.5f;
 
        // Bottom-left
        s_Data.VertexBufferPtr->Position = { InPosition.x - lHalfW, InPosition.y - lHalfH, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { 0.f, 0.f };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;
 
        // Bottom-right
        s_Data.VertexBufferPtr->Position = { InPosition.x + lHalfW, InPosition.y - lHalfH, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { 1.f, 0.f };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;
 
        // Top-right
        s_Data.VertexBufferPtr->Position = { InPosition.x + lHalfW, InPosition.y + lHalfH, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { 1.f, 1.f };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;
 
        // Top-left
        s_Data.VertexBufferPtr->Position = { InPosition.x - lHalfW, InPosition.y + lHalfH, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { 0.f, 1.f };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;
 
        ++s_Data.QuadCount;
    }

    void Renderer2D::DrawSprite(const Vector2F& InPosition, const Vector2F& InSize, const TextureHandle& InTexture,
        const Vector4F& InColor)
    {
        OPAAX_CORE_ASSERT(InTexture.IsValid())
        DrawSprite(InPosition, InSize, *InTexture.Get(), InColor);
    }

    void Renderer2D::DrawSprite(const Vector2F& InPosition, const Vector2F& InSize, const TextureHandle& InTexture,
        const Vector2F& InUVMin, const Vector2F& InUVMax, const Vector4F& InColor)
    {
        OPAAX_CORE_ASSERT(InTexture.IsValid())
        DrawSprite(InPosition, InSize, *InTexture.Get(), InUVMin, InUVMax, InColor);
    }

    void Renderer2D::DrawSprite(const Vector2F& InPosition,
                                const Vector2F& InSize,
                                OpenGLTexture2D& InTexture,
                                const Vector4F& InColor)
    {
        DrawSprite(InPosition, InSize, InTexture,
                   { 0.f, 0.f }, { 1.f, 1.f }, InColor);
    }
 
    void Renderer2D::DrawSprite(const Vector2F& InPosition,
                                 const Vector2F& InSize,
                                 OpenGLTexture2D& InTexture,
                                 const Vector2F& InUVMin,
                                 const Vector2F& InUVMax,
                                 const Vector4F& InColor)
    {
        if (s_Data.QuadCount >= MAX_QUADS)
        {
            Flush();
            StartBatch();
        }
 
        const float lTexIndex = GetTextureSlot(InTexture);
        const float lHalfW    = InSize.x * 0.5f;
        const float lHalfH    = InSize.y * 0.5f;
 
        // Bottom-left
        s_Data.VertexBufferPtr->Position = { InPosition.x - lHalfW, InPosition.y - lHalfH, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { InUVMin.x, InUVMin.y };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;
 
        // Bottom-right
        s_Data.VertexBufferPtr->Position = { InPosition.x + lHalfW, InPosition.y - lHalfH, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { InUVMax.x, InUVMin.y };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;
 
        // Top-right
        s_Data.VertexBufferPtr->Position = { InPosition.x + lHalfW, InPosition.y + lHalfH, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { InUVMax.x, InUVMax.y };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;
 
        // Top-left
        s_Data.VertexBufferPtr->Position = { InPosition.x - lHalfW, InPosition.y + lHalfH, 0.f };
        s_Data.VertexBufferPtr->Color    = InColor;
        s_Data.VertexBufferPtr->TexCoord = { InUVMin.x, InUVMax.y };
        s_Data.VertexBufferPtr->TexIndex = lTexIndex;
        ++s_Data.VertexBufferPtr;
 
        ++s_Data.QuadCount;
    }
 
} // namespace Opaax