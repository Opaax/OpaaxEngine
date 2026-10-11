#pragma once

#include <optional>

#include <nlohmann/json.hpp>

#include "Resources/ResourceFormat.h"
#include "Resources/DataAsset/DataAsset.h"
#include "Resources/DataAsset/DataAssetFile.h"

namespace Opaax
{
    // =============================================================================
    // DataAssetResource — any .opaaxdata, loaded without knowing its C++ type: the type name and the
    //   raw data. As<T>() reads it as a T the first time it is asked, then keeps that value.
    //   FailFast: data drives gameplay, so a broken file is a null, not a silent default.
    // =============================================================================
    struct DataAssetResource final
    {
        OpaaxStringID  Type;
        nlohmann::json Data = nlohmann::json::object();

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Data Asset", DataAssetFile::DATA_ASSET_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::FailFast;

        static std::optional<DataAssetResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            DataAssetFile::Contents lContents;
            if (!DataAssetFile::Load(OpaaxString(InPath), lContents))
            {
                return std::nullopt;   // DataAssetFile already logged why
            }

            DataAssetResource lResource;
            lResource.Type = lContents.Type;
            lResource.Data = Move(lContents.Data);
            return lResource;
        }

        static DataAssetResource Placeholder() { return DataAssetResource{}; }

        // =============================================================================
        // Typed access
        // =============================================================================

        /**
         * The data as a T. Null if this asset holds another type, or if a value has the wrong JSON type.
         * Read on first call, then cached (a Reload replaces the whole resource, so the cache follows).
         * Main thread only.
         */
        template<CDataAsset T>
        const T* As() const
        {
            if (Type != DataAssetTypeName<T>())
            {
                return nullptr;
            }

            if (m_View == nullptr || m_View->GetTypeId() != TypeIdOf<T>())
            {
                if (m_bViewFailed) { return nullptr; }

                m_View = ReadDataAsset<T>(Data);

                if (m_View == nullptr)
                {
                    m_bViewFailed = true;
                    return nullptr;
                }
            }

            return &static_cast<const TDataAssetObject<T>*>(m_View.get())->Value;
        }

    private:
        mutable TUniquePtr<IDataAssetObject> m_View;
        mutable bool                         m_bViewFailed = false;
    };
}
