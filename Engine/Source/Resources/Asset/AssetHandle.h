#pragma once

#include "Application/Services/IPaths.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Resources/Asset/AssetRef.h"
#include "Resources/Asset/AssetResource.h"
#include "Resources/ResourceManager.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Asset);

    // =============================================================================
    // TAssetHandle<T> — a loaded asset. Keeps its file loaded while held, and always reads the current
    //   version: saving the file in the editor reloads it in place. Do not keep the pointer Get()
    //   returns across a frame; keep the handle.
    // =============================================================================
    template<typename T>
    class TAssetHandle
    {
    public:
        TAssetHandle() = default;

        TAssetHandle(ResourceRef<AssetResource> InRef, OpaaxString InPath)
            : m_Ref(Move(InRef)), m_Path(Move(InPath))
        {
        }

        /** The asset, or null when its file did not load or holds another class (warned once). */
        const T* Get() const
        {
            const AssetResource* lResource = m_Ref.Get();

            if (lResource == nullptr || !lResource->Instance.IsValid())
            {
                return nullptr;
            }

            const T* lAsset = lResource->template Get<T>();

            if (lAsset == nullptr && !m_bWarned)
            {
                m_bWarned = true;
                OPAAX_LOG(LogAsset, Warn, "Asset '{}' holds a '{}', not a '{}'", m_Path.CStr(),
                          lResource->ClassName.CStr(), T::StaticClass().GetName().CStr());
            }

            return lAsset;
        }

        /** The asset, or T's default object when it is missing: for a type whose defaults make sense. */
        const T& GetOrDefault() const
        {
            const T* lAsset = Get();
            return lAsset != nullptr ? *lAsset : *static_cast<const T*>(T::StaticClass().GetDefaultObject());
        }

        /** True once its file is loaded, whatever it holds. */
        bool IsLoaded() const noexcept { return m_Ref.IsValid() && m_Ref.Get() != nullptr; }

        /** The asset-relative path it was loaded from. */
        const OpaaxString& GetPath() const noexcept { return m_Path; }

    private:
        ResourceRef<AssetResource> m_Ref;
        OpaaxString                m_Path;
        mutable bool               m_bWarned = false;
    };

    /**
     * Loads the asset at InAssetPath (asset-relative or a mount), or shares it if already loaded.
     * @return An empty handle for an empty path
     */
    template<typename T>
    TAssetHandle<T> LoadAsset(ResourceManager& InResources, const IPaths& InPaths, const OpaaxString& InAssetPath)
    {
        if (InAssetPath.IsEmpty())
        {
            return {};
        }

        const OpaaxString lAbsolute = InPaths.AssetToAbsolute(InAssetPath);
        return TAssetHandle<T>(InResources.Load<AssetResource>(lAbsolute.CStr()), InAssetPath);
    }

    /** Loads the asset InRef names. */
    template<typename T>
    TAssetHandle<T> LoadAsset(ResourceManager& InResources, const IPaths& InPaths, const TAssetRef<T>& InRef)
    {
        return LoadAsset<T>(InResources, InPaths, InRef.Path);
    }
}
