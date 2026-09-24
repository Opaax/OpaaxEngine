#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/Input/InputActionData.h"
#include "Engine/Subsystems/Resources/Types/Input/InputActionFile.h"

namespace Opaax
{
    // =============================================================================
    // InputActionResource — a `.opaaxaction` as a RESOURCE. MoveModeResource's shape.
    //
    //   PLACEHOLDER: a missing action yields the DEFAULTS — an unnamed Bool action. A mapping
    //   entry pointing at it is then refused by name at AddContext, loudly, instead of
    //   half-existing. Degraded and visible, which is the whole point of the policy.
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
                return std::nullopt;   // InputActionFile already logged which reason it was
            }

            return lResource;
        }

        /** An unnamed Bool action. Nothing binds to it, and AddContext says so. */
        static InputActionResource Placeholder() { return InputActionResource{}; }

        Uint64 ByteSize() const noexcept
        {
            return sizeof(InputActionResource)
                 + Data.Description.GetLength()
                 + Data.Modifiers.size() * sizeof(InputModifierData);
        }
    };
}
