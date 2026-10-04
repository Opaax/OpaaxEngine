#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    inline constexpr LogCategory LogDataAssetFile{"DataAssetFile"};

    // =============================================================================
    // DataAssetFile — reads and writes .opaaxdata files: { "Data": { ...fields... }, "Type": "JumpTuning" }.
    //   One extension for every data asset type; the type is inside the file.
    // =============================================================================
    namespace DataAssetFile
    {
        inline constexpr const char* DATA_ASSET_EXTENSION = ".opaaxdata";

        struct Contents
        {
            OpaaxStringID  Type;
            nlohmann::json Data = nlohmann::json::object();
        };

        /** The exact text Save writes (dump(4), sorted keys). Used by the editor's dirty check. */
        OPAAX_API OpaaxString Serialize(OpaaxStringID InType, const nlohmann::json& InData);

        /**
         * Writes a data asset, replacing any content.
         * @return False if the file could not be written
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, OpaaxStringID InType, const nlohmann::json& InData);

        /**
         * Reads a data asset's type and raw data. OutContents is untouched on failure.
         * @return False if the file is missing, not JSON, or has no "Type" string / "Data" object
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, Contents& OutContents);
    }
}
