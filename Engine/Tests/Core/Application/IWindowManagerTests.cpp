// Suite: IWindowManager. Verifies the pure config->props mapping and the null object,
// WITHOUT creating a real window — Window::Create spins up a GL/VK context the headless
// test runner can't provide, so CreateMainWindow() is never called on a real manager.
#include <doctest.h>

#include "Application/Services/Window/IWindowManager.h"
#include "Application/Services/Window/WindowManager.h"
#include "Application/Services/AppServiceLocator.h"
#include "Engine/Config/EngineConfigData.h"
#include "Core/String/OpaaxString.hpp"

using namespace Opaax;

TEST_CASE("MakeWindowProps: maps the engine config window fields 1:1")
{
    EngineConfigData lData;
    lData.WindowTitle  = OpaaxString("Test Title");
    lData.WindowWidth  = 1024;
    lData.WindowHeight = 768;
    lData.WindowMode   = OpaaxString("Borderless");

    const WindowProps lProps = MakeWindowProps(lData);
    CHECK(lProps.Title == "Test Title");
    CHECK(lProps.Width == 1024);
    CHECK(lProps.Height == 768);
    CHECK(lProps.Mode == WindowMode::Borderless);
}

TEST_CASE("WindowModeFromString: every mode round-trips, unknown falls back to Windowed")
{
    CHECK(WindowModeFromString(OpaaxString("Windowed"))   == WindowMode::Windowed);
    CHECK(WindowModeFromString(OpaaxString("Borderless")) == WindowMode::Borderless);
    CHECK(WindowModeFromString(OpaaxString("Fullscreen")) == WindowMode::Fullscreen);

    // Unknown and empty both fall back rather than refusing to open a window.
    CHECK(WindowModeFromString(OpaaxString("Maximized")) == WindowMode::Windowed);
    CHECK(WindowModeFromString(OpaaxString(""))          == WindowMode::Windowed);

    // The pair is what keeps a serialized config readable by the next boot.
    for (const WindowMode lMode : { WindowMode::Windowed, WindowMode::Borderless, WindowMode::Fullscreen })
    {
        CHECK(WindowModeFromString(OpaaxString(WindowModeToString(lMode))) == lMode);
    }
}

TEST_CASE("MakeWindowProps: an unknown config mode still yields a usable window")
{
    EngineConfigData lData;
    lData.WindowMode = OpaaxString("Borderles");   // typo — the realistic failure

    CHECK(MakeWindowProps(lData).Mode == WindowMode::Windowed);
}

TEST_CASE("IWindowManager: the null manager owns no window and is never null")
{
    AppServiceLocator lLocator;
    IWindowManager& lSys = lLocator.Get<IWindowManager>(); // unprovided -> NullWindowManager
    CHECK(lSys.IsNull());
    CHECK(&lSys == &IWindowManager::Null());

    CHECK(lSys.GetMainWindow()    == nullptr);
    CHECK(lSys.CreateMainWindow() == nullptr); // null manager never builds a window
    CHECK_FALSE(lSys.HasMainWindow());
}

TEST_CASE("WindowManager: a freshly constructed manager has no window (no GLFW at construction)")
{
    WindowManager lWm;
    CHECK(lWm.GetMainWindow() == nullptr);
    CHECK_FALSE(lWm.HasMainWindow());
}
