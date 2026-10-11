#pragma once

#include <concepts>
#include <optional>

#include "Core/OpaaxTypes.h"

// =============================================================================
// ResourceConcept — the compile-time contract for resource types (no base class).
//   A resource is a plain struct satisfying CResource; nothing else to register.
// =============================================================================
namespace Opaax
{
    // -------------------------------------------------------------------------
    // Failure policy, per type:
    //   Placeholder — a substitute is acceptable (pink texture, silent audio, ...).
    //   FailFast    — anything that drives logic (Level, gameplay data): a load failure
    //                 gives null, and propagates to whatever depends on it.
    // -------------------------------------------------------------------------
    enum class EFailPolicy : Uint8
    {
        Placeholder,
        FailFast
    };

    // -------------------------------------------------------------------------
    // Load state. Loading = async request placed, not published yet; it resolves to the
    // failure policy (placeholder/null) until then.
    // -------------------------------------------------------------------------
    enum class EResourceState : Uint8
    {
        Unloaded,
        Loading, // async request placed, not published yet
        Loaded,
        Failed
    };

    // Composite loading is threaded through LoadContext& (defined elsewhere).
    class LoadContext;

    // =============================================================================
    // CResource — the contract every resource type must satisfy.
    //   Load may run on any thread: file IO and CPU decode only (no GPU, no shared state).
    //   The optional Initialize() runs on the main thread after Load (GPU upload).
    // =============================================================================
    template<typename T>
    concept CResource = requires(const char* InPath, LoadContext& InCtx)
    {
        // Full object or nothing: no exceptions, no partial resources.
        { T::Load(InPath, InCtx) } -> std::same_as<std::optional<T>>;
        // Built once per pool (pink texture, silent clip, empty table, ...).
        { T::Placeholder() }       -> std::same_as<T>;
        // Placeholder vs FailFast.
        { T::FailPolicy }          -> std::convertible_to<EFailPolicy>;

        // Optional, detected at compile time:
        //   void   Initialize()      — main-thread GPU upload, run once.
        //   Uint64 ByteSize() const  — payload size in bytes (else sizeof(T)).
    };
}
