#pragma once

#include <optional>

#include "Core/OpaaxTypes.h"
#include "Core/IO/FileIO.h"
#include "Application/Services/ILogger.h"
#include "Engine/Subsystems/Resources/ResourceConcept.hpp"

// =============================================================================
// BinaryResource — the reference CResource: a whole file loaded as raw bytes.
// No GPU, no dependencies — the simplest thing that satisfies the contract, and
// the M-RES-1 proving ground. Placeholder policy: a missing binary degrades to
// an empty blob rather than failing the caller.
// =============================================================================
namespace Opaax
{
    
    struct BinaryResource final
    {
        TDynArray<Uint8> Bytes;

        // ---- CResource contract --------------------------------------------------
        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<BinaryResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            BinaryResource lResource;
            if (!FileIO::ReadAllBytes(OpaaxString(InPath), lResource.Bytes))
            {
                OPAAX_ENGINE_LOG(Error, "BinaryResource: cannot read '{}'", InPath)
                return std::nullopt;
            }

            return lResource;
        }

        static BinaryResource Placeholder() { return BinaryResource{}; } // empty blob

        // Optional bytes accounting hook picked up by ResourcePool (if-constexpr).
        Uint64 ByteSize() const noexcept { return static_cast<Uint64>(Bytes.size()); }
    };
}
