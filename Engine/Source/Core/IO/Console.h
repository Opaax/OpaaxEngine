#pragma once

#include "Core/String/OpaaxStringView.hpp"

// =============================================================================
// Console — the process's standard output, for tools and benchmarks that print results. Engine
//   messages go through OPAAX_LOG.
// =============================================================================
namespace Opaax::Console
{
    /** Writes InText and a line break to the standard output, and flushes it at once. */
    void WriteLine(OpaaxStringView InText);
}
