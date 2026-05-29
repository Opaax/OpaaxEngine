#include "Texture2D.h"

#include "RHI/Texture.h"

namespace Opaax
{
    // =============================================================================
    // CTORS - DTOR
    // =============================================================================
    Texture2D::Texture2D(const OpaaxString& InSourcePath, OpaaxStringID InAssetID)
        : m_AssetID(InAssetID)
        , m_SourcePath(InSourcePath)
        , m_State(EAssetState::Loading)
        , m_Gpu(ITexture2D::Create(InSourcePath.CStr()))
    {
        m_State = (m_Gpu && m_Gpu->IsLoaded()) ? EAssetState::Loaded : EAssetState::Failed;
    }

    Texture2D::Texture2D(Uint32 InWidth, Uint32 InHeight)
        : m_State(EAssetState::Loading)
        , m_Gpu(ITexture2D::Create(InWidth, InHeight))
    {
        m_State = (m_Gpu && m_Gpu->IsLoaded()) ? EAssetState::Loaded : EAssetState::Failed;
    }

    Texture2D::Texture2D(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
        : m_State(EAssetState::Loading)
        , m_Gpu(ITexture2D::Create(InData, InWidth, InHeight, InChannels))
    {
        m_State = (m_Gpu && m_Gpu->IsLoaded()) ? EAssetState::Loaded : EAssetState::Failed;
    }

    Texture2D::~Texture2D() = default;

    // =============================================================================
    // Functions
    // =============================================================================
    void Texture2D::Bind(Uint32 InSlot) const { if (m_Gpu) { m_Gpu->Bind(InSlot); } }
    void Texture2D::Unbind()            const { if (m_Gpu) { m_Gpu->Unbind();     } }

    Uint32 Texture2D::GetWidth()      const noexcept { return m_Gpu ? m_Gpu->GetWidth()      : 0u; }
    Uint32 Texture2D::GetHeight()     const noexcept { return m_Gpu ? m_Gpu->GetHeight()     : 0u; }
    Uint32 Texture2D::GetRendererID() const noexcept { return m_Gpu ? m_Gpu->GetRendererID() : 0u; }
    bool   Texture2D::IsLoaded()      const noexcept { return m_Gpu && m_Gpu->IsLoaded();          }
}
