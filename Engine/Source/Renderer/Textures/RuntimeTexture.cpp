#include "Renderer/Textures/RuntimeTexture.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Core/Maths/Maths.h"
#include "Object/ObjectPtr.h"
#include "Renderer/Textures/Texture2D.h"
#include "Renderer/Textures/TexturePaint.h"
#include "RHI/BlendState.h"

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

    bool RuntimeTexture::Paint(const TDynArray<PaintStamp>& InStamps, const EPaintMode InMode)
    {
        if (!IsPaintable())
        {
            OPAAX_LOG(LogTexture2D, Warn, "'{}' cannot be painted: an empty handle, or a texture not made with bPaintable",
                      m_Path.CStr());
            return false;
        }

        const Uint32 lDropped = OpaaxApplication::GetAppService<IEngine>().PaintTexture(*this, InStamps, InMode);
        if (lDropped > 0)
        {
            OPAAX_LOG(LogTexture2D, Warn, "'{}': {} of {} stamps draw nothing (a size not above 0, or not a number) "
                      "and were dropped", m_Path.CStr(), lDropped, InStamps.size());
        }
        return true;
    }

    bool RuntimeTexture::Clear(const Vector4F& InColor)
    {
        return ClearRegion(TextureRegion::Whole(GetWidth(), GetHeight()), InColor);
    }

    bool RuntimeTexture::ClearRegion(const TextureRegion& InRegion, const Vector4F& InColor)
    {
        if (!IsPaintable())
        {
            OPAAX_LOG(LogTexture2D, Warn, "'{}' cannot be cleared: an empty handle, or a texture not made with bPaintable",
                      m_Path.CStr());
            return false;
        }

        const TOptional<TextureRegion> lInside = ClipRegion(InRegion, GetWidth(), GetHeight());
        if (!lInside.has_value())
        {
            OPAAX_LOG(LogTexture2D, Warn, "'{}': the region {},{} {}x{} is outside its {}x{} texels — nothing cleared",
                      m_Path.CStr(), InRegion.X, InRegion.Y, InRegion.Width, InRegion.Height, GetWidth(), GetHeight());
            return false;
        }

        // The texture holds premultiplied colour.
        const Vector4F lColor{ Maths::Clamp(InColor.r, 0.f, 1.f), Maths::Clamp(InColor.g, 0.f, 1.f),
                               Maths::Clamp(InColor.b, 0.f, 1.f), Maths::Clamp(InColor.a, 0.f, 1.f) };
        OpaaxApplication::GetAppService<IEngine>().ClearTexture(*this, *lInside, Premultiply(lColor));
        return true;
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
