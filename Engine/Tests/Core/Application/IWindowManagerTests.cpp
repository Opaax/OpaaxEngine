// Suite: IWindowManager. Verifies the pure config->props mapping and the null object,
// WITHOUT creating a real window — Window::Create spins up a GL/VK context the headless
// test runner can't provide, so CreateMainWindow() is never called on a real manager.
#include <doctest.h>

#include "Core/Application/Services/IWindowManager.h"
#include "Core/Application/Services/AppServiceLocator.h"
#include "Core/Config/EngineConfigData.h"
#include "Core/OpaaxString.hpp"

using namespace Opaax;

TEST_CASE("MakeWindowProps: maps the engine config window fields 1:1")
{
    EngineConfigData lData;
    lData.WindowTitle  = OpaaxString("Test Title");
    lData.WindowWidth  = 1024;
    lData.WindowHeight = 768;

    const WindowProps lProps = MakeWindowProps(lData);
    CHECK(lProps.Title == "Test Title");
    CHECK(lProps.Width == 1024);
    CHECK(lProps.Height == 768);
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
