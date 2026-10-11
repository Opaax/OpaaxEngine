#include "__NAME__App.h"

__NAME__App::__NAME__App(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
}

void __NAME__App::OnInitializeApplication()
{
    // Intentionally empty: __NAME__ registers no app-level config. Overridden so the base's
    // "not overridden" trace does not fire.
}
