#pragma once

#include "Application/OpaaxApplication.h"

// =============================================================================
// TetrisApp — the runtime host: a thin layer over OpaaxApplication. The game module's types
// register themselves (OPAAX_REGISTER_* macros).
// The startup world is created last, from the .opaaxproj's "startupLevel" (default "Main").
// =============================================================================
class TetrisApp : public Opaax::OpaaxApplication
{
public:
    TetrisApp(int InArgc, char** InArgv);

protected:
    void OnInitializeApplication() override;
};
