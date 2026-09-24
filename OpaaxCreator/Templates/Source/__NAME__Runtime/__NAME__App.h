#pragma once

#include "Application/OpaaxApplication.h"

// =============================================================================
// __NAME__App — the runtime host: a thin composition root over OpaaxApplication. It routes the
// game module in; the engine does the rest (BO4 — the startup world is created LAST, named from
// the .opaaxproj's "startupLevel", falling back to "Main").
// =============================================================================
class __NAME__App : public Opaax::OpaaxApplication
{
public:
    __NAME__App(int InArgc, char** InArgv);

protected:
    void OnInitializeApplication() override;
    void RegisterModules(Opaax::ModuleRegistrar& InRegistrar) override;
};
