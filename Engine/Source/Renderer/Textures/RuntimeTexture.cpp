#include "Renderer/Textures/RuntimeTexture.h"

#include "Application/Services/IPaths.h"
#include "Object/ObjectPtr.h"
#include "Renderer/Textures/Texture2D.h"

namespace Opaax
{
    RuntimeTexture RuntimeTexture::Create(ResourceManager& InResources, const OpaaxString& InPath,
                                          const RuntimeTextureDesc& InDesc, const Uint8* InPixels)
    {
        if (!IsRuntimePath(OpaaxStringView(InPath)))
        {
            OPAAX_LOG(LogTexture2D, Error, "'{}' is not a runtime path: a texture made in code is named \"{}<Name>\"",
                      InPath.CStr(), RUNTIME_MOUNT);
            return RuntimeTexture{};
        }

        if (InDesc.bPaintable && InDesc.Channels != 4)
        {
            OPAAX_LOG(LogTexture2D, Error, "'{}': a paintable texture has 4 channels, not {}", InPath.CStr(), InDesc.Channels);
            return RuntimeTexture{};
        }

        TStrongObjectPtr<Texture2D> lTexture = NewObject<Texture2D>(OpaaxStringID(InPath.CStr()));
        const bool bMade = InDesc.bPaintable
                         ? lTexture->InitPaintable(InDesc.Width, InDesc.Height, InPixels)
                         : lTexture->InitFromPixels(InDesc.Width, InDesc.Height, InDesc.Channels, InPixels, InDesc.bSmooth);
        if (!bMade)
        {
            return RuntimeTexture{};
        }

        AssetResource lResource;
        lResource.ClassName = Texture2D::StaticClass().GetName();
        lResource.Instance  = Move(lTexture);

        // Added initializes it: the pixels go to the GPU now.
        RuntimeTexture lHandle;
        lHandle.m_Ref = InResources.Add<AssetResource>(InPath.CStr(), Move(lResource));
        if (!lHandle.m_Ref.IsValid())
        {
            return RuntimeTexture{};
        }

        lHandle.m_Path = InPath;
        return lHandle;
    }

    bool RuntimeTexture::Update(const Uint8* InPixels)
    {
        Texture2D* lTexture = GetTexture();
        return lTexture != nullptr
            && lTexture->UpdatePixels(TextureRegion::Whole(lTexture->GetWidth(), lTexture->GetHeight()), InPixels);
    }

    bool RuntimeTexture::UpdateRegion(const TextureRegion& InRegion, const Uint8* InPixels)
    {
        Texture2D* lTexture = GetTexture();
        return lTexture != nullptr && lTexture->UpdatePixels(InRegion, InPixels);
    }

    void RuntimeTexture::Reset()
    {
        m_Ref = ResourceRef<AssetResource>{};
        m_Path = OpaaxString();
    }

    bool RuntimeTexture::IsValid() const
    {
        return GetTexture() != nullptr;
    }

    Uint32 RuntimeTexture::GetWidth() const
    {
        const Texture2D* lTexture = GetTexture();
        return lTexture != nullptr ? lTexture->GetWidth() : 0u;
    }

    Uint32 RuntimeTexture::GetHeight() const
    {
        const Texture2D* lTexture = GetTexture();
        return lTexture != nullptr ? lTexture->GetHeight() : 0u;
    }

    Int32 RuntimeTexture::GetChannels() const
    {
        const Texture2D* lTexture = GetTexture();
        return lTexture != nullptr ? lTexture->GetChannels() : 0;
    }

    bool RuntimeTexture::IsPaintable() const
    {
        const Texture2D* lTexture = GetTexture();
        return lTexture != nullptr && lTexture->IsPaintable();
    }

    Texture2D* RuntimeTexture::GetTexture() const
    {
        if (!m_Ref.IsValid())
        {
            return nullptr;
        }

        AssetResource* lResource = m_Ref.Get();
        return (lResource != nullptr && lResource->Instance.IsValid()) ? Cast<Texture2D>(lResource->Instance.Get())
                                                                       : nullptr;
    }
}
