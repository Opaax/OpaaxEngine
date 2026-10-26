// LegacyAssetFile.h — an old per-type asset file brought to .opaaxasset the way --upgrade-assets does
//   it, then loaded: what a test of a class's old format checks the values of.
#pragma once

#include "Object/ObjectPtr.h"

namespace Opaax
{
    class Asset;
    class ObjectClassRegistry;
    class TestTempDir;

    /**
     * InOldText, the text of a file named InOldName in an old format, converted and written under InDir
     * at its converted name, then loaded with InClasses. Fails the case if the format is unknown or the
     * text does not convert.
     */
    TStrongObjectPtr<Asset> ConvertAndLoad(const TestTempDir& InDir, const char* InOldName, const char* InOldText,
                                           const ObjectClassRegistry& InClasses);
}
