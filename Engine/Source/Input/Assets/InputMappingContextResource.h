#pragma once

#include <optional>

#include "Resources/ResourceFormat.h"
#include "Input/Assets/InputMappingContextData.h"
#include "Input/Assets/InputMappingContextFile.h"

namespace Opaax
{
    // =============================================================================
    // InputMappingContextResource — a .opaaxinputmap as a resource.
    //   Placeholder policy: an empty context (no bindings).
    // =============================================================================
    struct InputMappingContextResource final
    {
        InputMappingContextData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Input Mapping Context", InputMappingContextFile::INPUT_MAP_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<InputMappingContextResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            InputMappingContextResource lResource;
            if (!InputMappingContextFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // InputMappingContextFile already logged why
            }

            return lResource;
        }

        /** An empty context: no bindings. */
        static InputMappingContextResource Placeholder() { return InputMappingContextResource{}; }

        Uint64 ByteSize() const noexcept
        {
            return sizeof(InputMappingContextResource)
                 + Data.Mappings.size() * sizeof(InputMappingEntry);
        }
    };
}
