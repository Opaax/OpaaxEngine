#include "__NAME__App.h"

#include "__NAME__.h"

__NAME__App::__NAME__App(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
}

void __NAME__App::OnInitializeApplication()
{
    // Intentionally empty: __NAME__ registers no app-level config today. Kept as an override so
    // the base's "not overridden" trace does not fire.
}

void __NAME__App::RegisterModules(Opaax::ModuleRegistrar& InRegistrar)
{
    __NAME__Module().OnRegister(InRegistrar);
}
