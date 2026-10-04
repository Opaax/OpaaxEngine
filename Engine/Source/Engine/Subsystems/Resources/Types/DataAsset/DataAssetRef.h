#pragma once

#include "Application/Services/IPaths.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/DataAsset/DataAssetResource.h"

namespace Opaax
{
    inline constexpr LogCategory LogDataAsset{"DataAsset"};

    // =============================================================================
    // TDataAssetRef<T> — a field that points at a .opaaxdata holding a T. Saved as its path.
    //   Use it in a component or another data asset; load it with LoadDataAsset.
    // =============================================================================
    template<CDataAsset T>
    struct TDataAssetRef
    {
        using DataType = T;

        /** Asset-relative ("Data/Grunt.opaaxdata"). Empty: points at nothing. */
        OpaaxString Path;

        bool IsEmpty() const noexcept { return Path.IsEmpty(); }
    };

    template<CDataAsset T>
    void to_json(nlohmann::json& InJson, const TDataAssetRef<T>& InValue)
    {
        InJson = InValue.Path;
    }

    template<CDataAsset T>
    void from_json(const nlohmann::json& InJson, TDataAssetRef<T>& InValue)
    {
        InJson.get_to(InValue.Path);
    }

    // =============================================================================
    // TDataAssetHandle<T> — a loaded data asset. Keeps the file loaded while held, and always reads
    //   the current version: saving the file in the editor reloads it in place.
    // =============================================================================
    template<CDataAsset T>
    class TDataAssetHandle
    {
    public:
        TDataAssetHandle() = default;

        TDataAssetHandle(ResourceRef<DataAssetResource> InRef, OpaaxString InPath)
            : m_Ref(Move(InRef)), m_Path(Move(InPath))
        {
        }

        /** The value, or null if the file did not load or holds another type (warned once). */
        const T* Get() const
        {
            const DataAssetResource* lResource = m_Ref.Get();

            if (lResource == nullptr)
            {
                return nullptr;
            }

            const T* lValue = lResource->template As<T>();

            if (lValue == nullptr && !m_bWarned)
            {
                m_bWarned = true;
                OPAAX_LOG(LogDataAsset, Warn, "Data asset '{}' holds a '{}', not a '{}' (or a value has the wrong type)",
                          m_Path.CStr(), lResource->Type.IsValid() ? lResource->Type.CStr() : "?",
                          DataAssetTypeName<T>().CStr());
            }

            return lValue;
        }

        bool IsLoaded() const noexcept { return m_Ref.IsValid() && m_Ref.Get() != nullptr; }

    private:
        ResourceRef<DataAssetResource> m_Ref;
        OpaaxString                    m_Path;
        mutable bool                   m_bWarned = false;
    };

    /**
     * Loads the asset InRef points at.
     * @return An empty handle if InRef is empty
     */
    template<CDataAsset T>
    TDataAssetHandle<T> LoadDataAsset(ResourceManager& InResources, const IPaths& InPaths, const TDataAssetRef<T>& InRef)
    {
        if (InRef.IsEmpty())
        {
            return {};
        }

        const OpaaxString lAbsolute = InPaths.AssetToAbsolute(InRef.Path);
        return TDataAssetHandle<T>(InResources.Load<DataAssetResource>(lAbsolute.CStr()), InRef.Path);
    }
}
