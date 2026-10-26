#include "LegacyAssetFile.h"

#include <doctest.h>

#include "TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetFile.h"
#include "Resources/Asset/AssetMigration.h"

namespace Opaax
{
    TStrongObjectPtr<Asset> ConvertAndLoad(const TestTempDir& InDir, const char* InOldName, const char* InOldText,
                                           const ObjectClassRegistry& InClasses)
    {
        const AssetMigration::LegacyFormat* lFormat = AssetMigration::FindLegacyFormat(OpaaxStringView(InOldName));
        REQUIRE(lFormat != nullptr);

        OpaaxString lAssetText;
        REQUIRE(AssetMigration::ConvertText(OpaaxString(InOldText), *lFormat, lAssetText));

        const OpaaxString lPath = InDir.Sub(AssetMigration::ConvertedPath(OpaaxStringView(InOldName)));
        REQUIRE(FileIO::WriteAllText(lPath, lAssetText));
        return AssetFile::LoadAsset(lPath, InClasses);
    }
}
