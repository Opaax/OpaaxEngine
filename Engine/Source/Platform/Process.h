#pragma once

#include "Core/OpaaxTypes.h"
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

    /**
     * Shows InFolder in the system's file browser (Explorer, Finder, the desktop's file manager) and
     * returns without waiting for it.
     * @return False when InFolder is not a folder, or the system could not be asked to show it
     */
    bool OpenFolder(const OpaaxString& InFolder);

    /**
     * InName's value in this program's environment.
     * @return Empty when InName is not set
     */
    TOptional<OpaaxString> GetEnvironment(const OpaaxString& InName);

    /**
     * Sets InName in this program's environment, which the libraries it loads and the programs it starts
     * then read. An empty InValue removes InName (Windows cannot keep an empty variable). Call it before
     * other threads read the environment.
     * @return False when InName is empty or holds '=', or the system refused
     */
    bool SetEnvironment(const OpaaxString& InName, const OpaaxString& InValue);
}
