#include "Engine/Input/InputTypes.h"

namespace Opaax
{
    const char* ToString(EInputValueType InType) noexcept
    {
        switch (InType)
        {
        case EInputValueType::Bool:   return "Bool";
        case EInputValueType::Axis1D: return "Axis1D";
        case EInputValueType::Axis2D: return "Axis2D";
        }

        return "Unknown";
    }

    const char* ToString(EInputTrigger InTrigger) noexcept
    {
        switch (InTrigger)
        {
        case EInputTrigger::Started:   return "Started";
        case EInputTrigger::Triggered: return "Triggered";
        case EInputTrigger::Completed: return "Completed";
        case EInputTrigger::Hold:      return "Hold";
        }

        return "Unknown";
    }

    const char* ToString(EInputModifier InModifier) noexcept
    {
        switch (InModifier)
        {
        case EInputModifier::Negate:    return "Negate";
        case EInputModifier::Swizzle:   return "Swizzle";
        case EInputModifier::DeadZone:  return "DeadZone";
        case EInputModifier::Scalar:    return "Scalar";
        case EInputModifier::Normalize: return "Normalize";
        }

        return "Unknown";
    }
}
