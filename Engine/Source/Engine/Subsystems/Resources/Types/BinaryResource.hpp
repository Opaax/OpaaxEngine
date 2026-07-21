#pragma once

#include <fstream>
#include <optional>

#include "Core/OpaaxTypes.h"
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
            std::ifstream lFile(InPath, std::ios::binary | std::ios::ate);
            if (!lFile.is_open())
            {
                OPAAX_ENGINE_LOG(Error, "BinaryResource: cannot open '{}'", InPath)
                return std::nullopt;
            }

            const std::streamsize lSize = lFile.tellg();
            if (lSize < 0)
            {
                OPAAX_ENGINE_LOG(Error, "BinaryResource: cannot size '{}'", InPath)
                return std::nullopt;
            }

            BinaryResource lResource;
            lResource.Bytes.resize(static_cast<size_t>(lSize));

            lFile.seekg(0, std::ios::beg);
            if (lSize > 0 &&
                !lFile.read(reinterpret_cast<char*>(lResource.Bytes.data()), lSize))
            {
                OPAAX_ENGINE_LOG(Error, "BinaryResource: cannot size '{}'", InPath)
                return std::nullopt;
            }

            return lResource;
        }

        static BinaryResource Placeholder() { return BinaryResource{}; } // empty blob

        // Optional bytes accounting hook picked up by ResourcePool (if-constexpr).
        Uint64 ByteSize() const noexcept { return static_cast<Uint64>(Bytes.size()); }
    };
}
