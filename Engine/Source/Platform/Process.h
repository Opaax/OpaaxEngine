#pragma once

#include "Core/String/OpaaxString.hpp"

// =============================================================================
// Process — the running program and the programs it starts.
// =============================================================================
namespace Opaax::Process
{
    /** Ends the program at once, as a crash: no destructors, no exit handlers. */
    [[noreturn]] void Abort();

    /**
     * Runs InCommandLine in the system's shell and waits for it to end.
     * @return The command's exit code (0 is success)
     */
    Int32 RunCommandLine(const OpaaxString& InCommandLine);
}
