#pragma once

#include "Application/OpaaxApplication.h"

// =============================================================================
// __NAME__App — the runtime host: a thin layer over OpaaxApplication. The game module's types
// register themselves (OPAAX_REGISTER_* macros).
// The startup world is created last, from the .opaaxproj's "startupLevel" (default "Main").
// =============================================================================
class __NAME__App : public Opaax::OpaaxApplication
{
public:
    __NAME__App(int InArgc, char** InArgv);

protected:
    void OnInitializeApplication() override;
};
