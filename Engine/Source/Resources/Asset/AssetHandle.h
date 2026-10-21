#pragma once

#include "Application/Services/IPaths.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Resources/Asset/AssetMigration.h"
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
            static_assert(!TIsAbstract<T>, "GetOrDefault: an abstract class has no default object; use Get().");

            const T* lAsset = Get();
            return lAsset != nullptr ? *lAsset : *static_cast<const T*>(T::StaticClass().GetDefaultObject());
        }

        /** True once its file is loaded, whatever it holds. */
        bool IsLoaded() const noexcept { return m_Ref.IsValid() && m_Ref.Get() != nullptr; }

        /**
         * Where its file stands: Loading while a background load runs (LoadAssetAsync), then Loaded
         * (whatever the file holds). Anything else means there is nothing to read: an empty handle, or a
         * load that failed.
         */
        EResourceState GetState() const
        {
            return m_Ref.IsValid() ? m_Ref.GetManager()->template GetState<AssetResource>(m_Ref.GetHandle())
                                   : EResourceState::Unloaded;
        }

        /** The asset-relative path it was loaded from. */
        const OpaaxString& GetPath() const noexcept { return m_Path; }

    private:
        ResourceRef<AssetResource> m_Ref;
        OpaaxString                m_Path;
        mutable bool               m_bWarned = false;
    };

    namespace AssetHandleDetail
    {
        /**
         * InAssetPath, or the file an old format's path was converted to, with a warning: converting
         * the files rewrites the paths in data, not in code.
         */
        inline OpaaxString CurrentPath(const OpaaxString& InAssetPath)
        {
            OpaaxString lPath = AssetMigration::ConvertedPath(InAssetPath);
            if (lPath.IsEmpty())
            {
                return InAssetPath;
            }

            OPAAX_LOG(LogAsset, Warn, "'{}' names a file of an old format: loading '{}', what --upgrade-assets "
                      "converts it to. Update the path where it is written.", InAssetPath.CStr(), lPath.CStr());
            return lPath;
        }
    }

    /**
     * Loads the asset at InAssetPath (asset-relative or a mount), or shares it if already loaded. A path
     * to an old format's file loads the file it was converted to, with a warning.
     * @return An empty handle for an empty path
     */
    template<typename T>
    TAssetHandle<T> LoadAsset(ResourceManager& InResources, const IPaths& InPaths, const OpaaxString& InAssetPath)
    {
        if (InAssetPath.IsEmpty())
        {
            return {};
        }

        const OpaaxString lPath     = AssetHandleDetail::CurrentPath(InAssetPath);
        const OpaaxString lAbsolute = InPaths.AssetToAbsolute(lPath);
        return TAssetHandle<T>(InResources.Load<AssetResource>(lAbsolute.CStr()), lPath);
    }

    /**
     * Starts loading the asset at InAssetPath in the background, or shares it if already loaded: the game
     * does not wait. Get() is null until it is loaded; GetState() says when.
     * @return An empty handle for an empty path
     */
    template<typename T>
    TAssetHandle<T> LoadAssetAsync(ResourceManager& InResources, const IPaths& InPaths, const OpaaxString& InAssetPath)
    {
        if (InAssetPath.IsEmpty())
        {
            return {};
        }

        const OpaaxString lPath     = AssetHandleDetail::CurrentPath(InAssetPath);
        const OpaaxString lAbsolute = InPaths.AssetToAbsolute(lPath);
        return TAssetHandle<T>(InResources.LoadAsync<AssetResource>(lAbsolute.CStr()), lPath);
    }

    /**
     * The asset at InAssetPath when it is loaded already: never starts a load.
     * @return An empty handle otherwise (not loaded, or still loading)
     */
    template<typename T>
    TAssetHandle<T> FindAsset(ResourceManager& InResources, const IPaths& InPaths, const OpaaxString& InAssetPath)
    {
        if (InAssetPath.IsEmpty())
        {
            return {};
        }

        const OpaaxString          lPath     = AssetHandleDetail::CurrentPath(InAssetPath);
        const OpaaxString          lAbsolute = InPaths.AssetToAbsolute(lPath);
        ResourceRef<AssetResource> lRef      = InResources.Find<AssetResource>(lAbsolute.CStr());
        return lRef.IsValid() ? TAssetHandle<T>(Move(lRef), lPath) : TAssetHandle<T>();
    }

    /** Loads the asset InRef names. */
    template<typename T, EResourceLoad TLoad>
    TAssetHandle<T> LoadAsset(ResourceManager& InResources, const IPaths& InPaths, const TAssetRef<T, TLoad>& InRef)
    {
        return LoadAsset<T>(InResources, InPaths, InRef.Path);
    }
}
