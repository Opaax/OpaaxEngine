#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    inline constexpr LogCategory LogMapFile{"MapFile"};

    // =============================================================================
    // MapFile — reads and writes .opaaxmap files.
    //       World  <-MapSerializer/MapFactory->  MapData  <-MapJson->  JSON  <-MapFile->  file
    // =============================================================================
    namespace MapFile
    {
        /** File extension. */
        inline constexpr const char* MAP_EXTENSION = ".opaaxmap";

        /**
         * ".../Maps/Decor.opaaxmap" -> MapId("Decor").
         * @return An invalid id if the path has no stem
         */
        MapId StemId(const OpaaxString& InAbsPath);

        /**
         * Writes InData to InAbsPath, replacing any content. Creates missing directories.
         * @param InAbsPath Absolute UTF-8 path
         * @return False if the file could not be written
         */
        bool Save(const OpaaxString& InAbsPath, const MapData& InData);

        /**
         * Writes already-serialized text (avoids serializing twice).
         * @param InEntityCount For the log only
         */
        bool SaveText(const OpaaxString& InAbsPath, const OpaaxString& InText,
                                Uint64 InEntityCount);

        /**
         * Reads InAbsPath into OutData. OutData is untouched on failure.
         * @param InAbsPath Absolute UTF-8 path
         * @return False if the file is missing, unreadable, malformed, or a newer format version
         */
        bool Load(const OpaaxString& InAbsPath, MapData& OutData);
    }
}
