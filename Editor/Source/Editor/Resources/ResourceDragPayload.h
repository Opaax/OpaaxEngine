#pragma once

#include <cstring>   // memcpy

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // ResourceDragPayload — the bytes a dragged resource file carries, and who may accept them.
    //   Layout: [Uint32 TypeId][Uint32 SubTypeId][path bytes], not null-terminated.
    //   SubTypeId narrows a type that holds many kinds (a .opaaxdata's data type); 0 means none.
    //   Pure, so the accept rule is testable without ImGui.
    // =============================================================================
    namespace ResourceDragPayload
    {
        inline constexpr Uint64 HEADER_SIZE = 2 * sizeof(Uint32);

        struct Decoded
        {
            Uint32      TypeId    = 0;
            Uint32      SubTypeId = 0;
            OpaaxString AssetPath;
        };

        inline TDynArray<Uint8> Encode(const Uint32 InTypeId, const Uint32 InSubTypeId, const OpaaxString& InAssetPath)
        {
            TDynArray<Uint8> lBytes(HEADER_SIZE + InAssetPath.GetLength());
            std::memcpy(lBytes.data(), &InTypeId, sizeof(Uint32));
            std::memcpy(lBytes.data() + sizeof(Uint32), &InSubTypeId, sizeof(Uint32));
            std::memcpy(lBytes.data() + HEADER_SIZE, InAssetPath.CStr(), InAssetPath.GetLength());
            return lBytes;
        }

        /** False when InData is too short to be a payload. */
        inline bool Decode(const void* InData, const Uint64 InSize, Decoded& OutDecoded)
        {
            if (InData == nullptr || InSize < HEADER_SIZE)
            {
                return false;
            }

            const Uint8* lBytes = static_cast<const Uint8*>(InData);
            std::memcpy(&OutDecoded.TypeId, lBytes, sizeof(Uint32));
            std::memcpy(&OutDecoded.SubTypeId, lBytes + sizeof(Uint32), sizeof(Uint32));
            OutDecoded.AssetPath = OpaaxString(reinterpret_cast<const char*>(lBytes + HEADER_SIZE),
                                               static_cast<Uint32>(InSize - HEADER_SIZE));
            return true;
        }

        /**
         * Whether a drop target asking for InTypeId (and InSubTypeId, unless 0) wants this payload.
         */
        inline bool Accepts(const Decoded& InPayload, const Uint32 InTypeId, const Uint32 InSubTypeId) noexcept
        {
            return InPayload.TypeId == InTypeId && (InSubTypeId == 0 || InPayload.SubTypeId == InSubTypeId);
        }
    }
}
