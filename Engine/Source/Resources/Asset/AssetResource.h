#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/ObjectPtr.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetFile.h"
#include "Resources/ResourceFormat.h"

namespace Opaax
{
    class LoadContext;

    // =============================================================================
    // AssetResource — an .opaaxasset as a resource: loaded once, shared, reloaded in place when its file
    //   is saved, whatever its class. Get<T>() reads it as a T.
    //   Placeholder: a file that fails to load leaves no asset, so Get<T>() is null.
    // =============================================================================
    struct AssetResource final
    {
        /** The class the file names, kept even when it did not load. */
        OpaaxStringID ClassName;

        /** The asset, owned by the resource. Empty for the placeholder. */
        TStrongObjectPtr<Asset> Instance;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Asset", AssetFile::ASSET_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        /** Reads the file and makes its asset, with the classes the resource manager was given. */
        static TOptional<AssetResource> Load(const char* InPath, LoadContext& InCtx);

        static AssetResource Placeholder() { return AssetResource{}; }

        /** The asset's own size. */
        Uint64 ByteSize() const noexcept
        {
            return sizeof(AssetResource) + (Instance.IsValid() ? Instance->GetClass().GetSize() : 0);
        }

        // =============================================================================
        // Typed access
        // =============================================================================

        /** The asset as a T, or null (nothing loaded, or another class). */
        template<typename T>
        const T* Get() const noexcept
        {
            return Cast<T>(static_cast<const Object*>(Instance.Get()));
        }
    };
}
