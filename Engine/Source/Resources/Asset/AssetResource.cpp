#include "Resources/Asset/AssetResource.h"

#include "Core/String/OpaaxPathString.h"
#include "Resources/ResourceManager.h"   // and LoadContext, which must come through it

namespace Opaax
{
    TOptional<AssetResource> AssetResource::Load(const char* InPath, LoadContext& InCtx)
    {
        const ObjectClassRegistry* lClasses = InCtx.GetManager().GetObjectClasses();

        if (lClasses == nullptr)
        {
            OPAAX_LOG(LogAssetFile, Error, "Asset '{}' cannot load: the resource manager was given no class registry",
                      InPath);
            return NullOpt;
        }

        const OpaaxString lPath(InPath);

        AssetFile::Contents lContents;
        if (!AssetFile::Load(lPath, lContents))
        {
            return NullOpt;   // AssetFile logged why
        }

        OpaaxString lError;
        TStrongObjectPtr<Asset> lAsset = AssetFile::Instantiate(lContents, *lClasses,
                                                                OpaaxStringID(PathString::Stem(lPath).ToString()),
                                                                &lError);
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
