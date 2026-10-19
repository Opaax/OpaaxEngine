#pragma once

#include "Core/Memory/Memory.h"

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // ResourceDragPayload — the bytes a dragged resource file carries, and who may accept them.
    //   Layout: [Uint32 TypeId][Uint32 KindCount][Uint32 Kind]...[path bytes], not null-terminated.
    //   The kinds narrow a type that holds many: an asset's class, then each parent class (by name
    //   id), so a field for a base class accepts its subclasses. Pure, so the accept rule is testable
    //   without ImGui.
    // =============================================================================
    namespace ResourceDragPayload
    {
        inline constexpr Uint64 HEADER_SIZE = 2 * sizeof(Uint32);

        /** More kinds than this are dropped (a class chain is a few deep). */
        inline constexpr Uint32 MAX_KINDS = 16;

        struct Decoded
        {
            Uint32            TypeId = 0;
            TDynArray<Uint32> Kinds;
            OpaaxString       AssetPath;
        };

        inline TDynArray<Uint8> Encode(const Uint32 InTypeId, const TDynArray<Uint32>& InKinds,
                                       const OpaaxString& InAssetPath)
        {
            const Uint32 lKindCount = InKinds.size() < MAX_KINDS ? static_cast<Uint32>(InKinds.size()) : MAX_KINDS;
            const Uint64 lKindBytes = static_cast<Uint64>(lKindCount) * sizeof(Uint32);

            TDynArray<Uint8> lBytes(HEADER_SIZE + lKindBytes + InAssetPath.GetLength());
            Memory::Copy(lBytes.data(), &InTypeId, sizeof(Uint32));
            Memory::Copy(lBytes.data() + sizeof(Uint32), &lKindCount, sizeof(Uint32));

            if (lKindCount > 0)
            {
                Memory::Copy(lBytes.data() + HEADER_SIZE, InKinds.data(), lKindBytes);
            }

            Memory::Copy(lBytes.data() + HEADER_SIZE + lKindBytes, InAssetPath.CStr(), InAssetPath.GetLength());
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

            Uint32 lKindCount = 0;
            Memory::Copy(&OutDecoded.TypeId, lBytes, sizeof(Uint32));
            Memory::Copy(&lKindCount, lBytes + sizeof(Uint32), sizeof(Uint32));

            const Uint64 lKindBytes = static_cast<Uint64>(lKindCount) * sizeof(Uint32);
            if (lKindCount > MAX_KINDS || InSize < HEADER_SIZE + lKindBytes)
            {
                return false;
            }

            OutDecoded.Kinds.resize(lKindCount);
            if (lKindCount > 0)
            {
                Memory::Copy(OutDecoded.Kinds.data(), lBytes + HEADER_SIZE, lKindBytes);
            }

            OutDecoded.AssetPath = OpaaxString(reinterpret_cast<const char*>(lBytes + HEADER_SIZE + lKindBytes),
                                               static_cast<Uint32>(InSize - HEADER_SIZE - lKindBytes));
            return true;
        }

        /**
         * Whether a drop target asking for InTypeId (and the kind InKind, unless 0) wants this payload.
         */
        inline bool Accepts(const Decoded& InPayload, const Uint32 InTypeId, const Uint32 InKind) noexcept
        {
            if (InPayload.TypeId != InTypeId)
            {
                return false;
            }

            if (InKind == 0)
            {
                return true;
            }

            for (const Uint32 lKind : InPayload.Kinds)
            {
                if (lKind == InKind) { return true; }
            }

            return false;
        }
    }
}
