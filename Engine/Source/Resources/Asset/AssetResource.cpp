#include "Resources/Asset/AssetResource.h"

#include "Core/String/OpaaxPathString.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/ResourceFormat.h"    // NormalizeExtension
#include "Resources/ResourceManager.h"   // and LoadContext, which must come through it

namespace Opaax
{
    namespace
    {
        /** A file of a class's own format (a .png): an asset of the class that reads InExtension. */
        TOptional<AssetResource> LoadSource(const char* InPath, const OpaaxStringID InExtension,
                                            const OpaaxStringID InName, const ObjectClassRegistry& InClasses)
        {
            const ObjectClass* lClass = InClasses.FindBySourceExtension(InExtension);
            if (lClass == nullptr)
            {
                OPAAX_LOG(LogAssetFile, Error, "'{}' did not load: no asset class reads '{}' files (OPAAX_ASSET_SOURCE)",
                          InPath, InExtension);
                return NullOpt;
            }

            TStrongObjectPtr<Asset> lAsset = Cast<Asset>(lClass->CreateInstance(InName));
            if (!lAsset.IsValid())
            {
                OPAAX_LOG(LogAssetFile, Error, "'{}' did not load: '{}' reads '{}' files but is not a concrete Asset",
                          InPath, lClass->GetName(), InExtension);
                return NullOpt;
            }

            if (!lAsset->LoadSource(InPath))
            {
                OPAAX_LOG(LogAssetFile, Error, "'{}' could not be read as a {}", InPath, lClass->GetName());
                return NullOpt;
            }

            AssetResource lResource;
            lResource.ClassName = lClass->GetName();
            lResource.Instance  = Move(lAsset);
            return lResource;
        }
    }

    TOptional<AssetResource> AssetResource::Load(const char* InPath, LoadContext& InCtx)
    {
        const ObjectClassRegistry* lClasses = InCtx.GetManager().GetObjectClasses();

        if (lClasses == nullptr)
        {
            OPAAX_LOG(LogAssetFile, Error, "Asset '{}' cannot load: the resource manager was given no class registry",
                      InPath);
            return NullOpt;
        }

        const OpaaxString   lPath(InPath);
        const OpaaxStringID lName(PathString::Stem(lPath).ToString());

        static const OpaaxStringID s_AssetExtension = NormalizeExtension(AssetFile::ASSET_EXTENSION);
        const OpaaxStringID        lExtension       = NormalizeExtension(PathString::Extension(lPath));

        if (lExtension != s_AssetExtension)
        {
            return LoadSource(InPath, lExtension, lName, *lClasses);
        }

        AssetFile::Contents lContents;
        if (!AssetFile::Load(lPath, lContents))
        {
            return NullOpt;   // AssetFile logged why
        }

        OpaaxString lError;
        TStrongObjectPtr<Asset> lAsset = AssetFile::Instantiate(lContents, *lClasses, lName, &lError);
        if (!lAsset.IsValid())
        {
            OPAAX_LOG(LogAssetFile, Error, "Asset '{}' did not load: {}", InPath, lError.CStr());
            return NullOpt;
        }

        AssetResource lResource;
        lResource.ClassName = lContents.Class;
        lResource.Instance  = Move(lAsset);
        return lResource;
    }
}
