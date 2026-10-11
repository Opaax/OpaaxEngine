#pragma once

#include <optional>

#include "Resources/ResourceFormat.h"
#include "Input/Assets/InputActionData.h"
#include "Input/Assets/InputActionFile.h"

namespace Opaax
{
    // =============================================================================
    // InputActionResource — a .opaaxaction as a resource.
    //   Placeholder policy: a missing action gives the defaults (an unnamed Bool action),
    //   which AddContext refuses with a warning.
    // =============================================================================
    struct InputActionResource final
    {
        InputActionData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Input Action", InputActionFile::INPUT_ACTION_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<InputActionResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            InputActionResource lResource;
            if (!InputActionFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // InputActionFile already logged why
            }

            return lResource;
        }

        /** An unnamed Bool action. */
        static InputActionResource Placeholder() { return InputActionResource{}; }

        Uint64 ByteSize() const noexcept
        {
            return sizeof(InputActionResource)
                 + Data.Description.GetLength()
                 + Data.Modifiers.size() * sizeof(InputModifierData);
        }
    };
}
