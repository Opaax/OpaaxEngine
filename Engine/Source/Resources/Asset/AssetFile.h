#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/ObjectPtr.h"

namespace Opaax
{
    class Asset;
    class ObjectClassRegistry;

    OPAAX_LOG_CATEGORY(AssetFile);

    // =============================================================================
    // AssetFile — reads and writes .opaaxasset files, the one file format of every asset type:
    //
    //   { "Class": "MoveMode", "Properties": { ...fields by name... }, "Version": 1 }
    //
    //   A missing property keeps its default, so a short file written by hand is enough.
    // =============================================================================
    namespace AssetFile
    {
        inline constexpr const char* ASSET_EXTENSION = ".opaaxasset";

        /** The format this engine writes. A file from a newer one is refused. */
        inline constexpr Int32 FORMAT_VERSION = 1;

        /** A file's contents, before (or without) a class to read them into. */
        struct Contents
        {
            OpaaxStringID  Class;
            nlohmann::json Properties = nlohmann::json::object();
            Int32          Version    = FORMAT_VERSION;
        };

        /** The exact text Save writes (dump(4), sorted keys). Used by the editor's dirty check. */
        OpaaxString Serialize(OpaaxStringID InClass, const nlohmann::json& InProperties);

        /** InAsset's class and properties as Save writes them. */
        OpaaxString Serialize(const Asset& InAsset);

        /**
         * Writes InAsset, replacing any content.
         * @return False if the file could not be written
         */
        bool Save(const OpaaxString& InAbsPath, const Asset& InAsset);

        /**
         * Reads a file's text into OutContents (untouched on failure).
         * @param OutError Why it failed (optional)
         * @return False when InText is not an object with a "Class" name and a "Properties" object, or its
         *   version is newer than FORMAT_VERSION
         */
        bool Parse(const OpaaxString& InText, Contents& OutContents, OpaaxString* OutError = nullptr);

        /**
         * Reads InAbsPath into OutContents (untouched on failure).
         * @return False when the file is missing, or Parse refuses it (logged)
         */
        bool Load(const OpaaxString& InAbsPath, Contents& OutContents);

        /**
         * The class an asset file's text names, when that name is already known (a registered class):
         * never adds a name, so it suits scanning many files. Logs nothing.
         * @return Invalid for text that is not an asset file, or an unknown class
         */
        OpaaxStringID FindClassName(const OpaaxString& InText);

        /**
         * A new asset of the class InContents names, its properties read from them.
         * @param InName The asset's name (its file's stem)
         * @param OutError Why it failed (optional)
         * @return Empty when the class is unknown, abstract or not an Asset, or a value has the wrong type
         */
        TStrongObjectPtr<Asset> Instantiate(const Contents& InContents, const ObjectClassRegistry& InClasses,
                                            OpaaxStringID InName, OpaaxString* OutError = nullptr);

        /**
         * Load, then Instantiate, named after the file.
         * @return Empty on failure (logged)
         */
        TStrongObjectPtr<Asset> LoadAsset(const OpaaxString& InAbsPath, const ObjectClassRegistry& InClasses);
    }
}
