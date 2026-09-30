#pragma once

#include <optional>

#include "Core/OpaaxTypes.h"
#include "Core/IO/FileIO.h"
#include "Core/Log/Logger.h"
#include "Engine/Subsystems/Resources/ResourceConcept.hpp"

// =============================================================================
// BinaryResource — a whole file loaded as raw bytes. The simplest resource type.
// Placeholder policy: a missing file gives an empty blob.
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
                OPAAX_ENGINE_LOG(Error, "BinaryResource: cannot read '{}'", InPath);
                return std::nullopt;
            }

            return lResource;
        }

        static BinaryResource Placeholder() { return BinaryResource{}; } // empty blob

        // Optional size hook, used by ResourcePool.
        Uint64 ByteSize() const noexcept { return static_cast<Uint64>(Bytes.size()); }
    };
}
