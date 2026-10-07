#pragma once

#include "Application/OpaaxApplication.h"

// =============================================================================
// TestWorldApp — the runtime host: a thin layer over OpaaxApplication. The game module's types
// register themselves (OPAAX_REGISTER_* macros).
// The startup world is created last, from the .opaaxproj's "startupLevel" (default "Main").
// =============================================================================
class TestWorldApp : public Opaax::OpaaxApplication
{
public:
    TestWorldApp(int InArgc, char** InArgv);

protected:
    void OnInitializeApplication() override;
};
