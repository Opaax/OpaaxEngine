#pragma once

#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/AssetFile.h"
#include "Resources/ResourceFormat.h"   // NormalizeExtension

namespace Opaax::Editor
{
    /**
     * The asset class of the file at InAbsPath: the one an .opaaxasset names when that name is known
     * (AssetFile::FindClassName), or the one that reads its extension. Invalid for any other file.
     * @param InClasses The classes that read files of their own extensions (null: only .opaaxasset files
     *                  get a class)
     */
    inline OpaaxStringID FindAssetClass(const OpaaxString& InAbsPath, const ObjectClassRegistry* InClasses)
    {
        static const OpaaxStringID s_AssetExtension = NormalizeExtension(AssetFile::ASSET_EXTENSION);
        const OpaaxStringID        lExtension       = NormalizeExtension(PathString::Extension(InAbsPath));

        // An .opaaxasset names its class; another file's class is the one that reads its extension.
        if (lExtension == s_AssetExtension)
        {
            return AssetFile::FindClassName(FileIO::ReadAllText(InAbsPath));
        }

        const ObjectClass* lSourceClass = InClasses != nullptr ? InClasses->FindBySourceExtension(lExtension) : nullptr;
        return lSourceClass != nullptr ? lSourceClass->GetName() : OpaaxStringID();
    }
}
