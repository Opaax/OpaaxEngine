#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextData.h"
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextFile.h"

namespace Opaax
{
    // =============================================================================
    // InputMappingContextResource — a `.opaaxinputmap` as a RESOURCE. MoverResource's shape.
    //
    //   PLACEHOLDER: a missing context yields an EMPTY one, which adds no bindings and leaves
    //   every action reading zero. The game runs and does not respond, rather than failing to
    //   boot — degraded and visible, and AddContext logs how many bindings it accepted so "0"
    //   is on the screen rather than inferred.
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
                return std::nullopt;   // InputMappingContextFile already logged which reason it was
            }

            return lResource;
        }

        /** An empty context: no bindings, so nothing is driven and nothing is consumed. */
        static InputMappingContextResource Placeholder() { return InputMappingContextResource{}; }

        Uint64 ByteSize() const noexcept
        {
            return sizeof(InputMappingContextResource)
                 + Data.Mappings.size() * sizeof(InputMappingEntry);
        }
    };
}
