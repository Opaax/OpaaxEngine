#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax::ECS
{
    // =============================================================================
    // ComponentTypeID
    //
    // Assigns a unique Uint32 to each component type at first use.
    // Cross-TU safe — uses a single atomic counter in a .cpp.
    //
    // NOTE: IDs are assigned at runtime on first GetTypeID<T>() call.
    //   Order depends on call order — do NOT serialize these IDs.
    //   They are session-local identifiers only.
    // =============================================================================

    namespace Internal
    {
        // Defined in ComponentTypeID.cpp — single definition, no ODR issues.
        Uint32 AllocateTypeID() noexcept;
    }

    template<typename T>
    Uint32 GetComponentTypeID() noexcept
    {
        // NOTE: Static local — initialized once per type, per process.
        //   Thread-safe by C++11 (magic statics).
        //   AllocateTypeID() uses an atomic increment — safe if two threads
        //   race on the first call for the same T.
        static const Uint32 s_ID = Internal::AllocateTypeID();
        return s_ID;
    }

} // namespace Opaax::ECS