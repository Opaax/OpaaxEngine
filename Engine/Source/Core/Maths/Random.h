#pragma once

#include "Core/OpaaxTypes.h"

// =============================================================================
// Random — numbers nobody can predict, from a generator the system seeds once per thread. For
//   identities (Guid::New). A game or a test that must replay uses a seeded generator instead.
// =============================================================================
namespace Opaax::Random
{
    /** 64 random bits. */
    Uint64 NextUint64() noexcept;
}
