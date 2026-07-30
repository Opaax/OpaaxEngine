#include "__NAME__.h"

#include "Engine/Modules/ModuleRegistrar.h"

void __NAME__Module::OnRegister(Opaax::ModuleRegistrar& InRegistrar)
{
    // Register the game's types here. A component is a plain struct satisfying CComponent
    // (one NLOHMANN_DEFINE_TYPE_INTRUSIVE — no base class), e.g.:
    //
    //   InRegistrar.Components().Register<MyComponent>();
    //   InRegistrar.WorldSubsystems().Register<MyWorldSubsystem>();
    (void)InRegistrar;
}
