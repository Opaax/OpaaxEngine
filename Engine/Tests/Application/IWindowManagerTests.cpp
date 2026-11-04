// Suite: IWindowManager. Verifies the pure config->props mapping and the null object,
// WITHOUT creating a real window — Window::Create spins up a GL/VK context the headless
// test runner can't provide, so CreateMainWindow() is never called on a real manager.
#include <doctest.h>

#include "Application/Services/IWindowManager.h"
#include "Platform/Window/WindowManager.h"
#include "Application/Services/AppServiceLocator.h"
#include "Engine/Config/EngineConfigData.h"
#include "Core/String/OpaaxString.hpp"

using namespace Opaax;

TEST_CASE("MakeWindowProps: maps the engine config window fields 1:1")
{
    EngineConfigData lData;
    lData.Window.Title  = OpaaxString("Test Title");
    lData.Window.Width  = 1024;
    lData.Window.Height = 768;
    lData.Window.Mode   = EWindowMode::Borderless;

    const WindowProps lProps = MakeWindowProps(lData);
    CHECK(lProps.Title == "Test Title");
    CHECK(lProps.Width == 1024);
    CHECK(lProps.Height == 768);
    CHECK(lProps.Mode == EWindowMode::Borderless);
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

TEST_CASE("IsOtherWindowShape: a new mode or size moves the window; a new title or vsync does not")
{
    WindowProps lShown;
    lShown.Width  = 1280;
    lShown.Height = 720;
    lShown.Mode   = EWindowMode::Windowed;

    WindowProps lWanted = lShown;
    CHECK_FALSE(IsOtherWindowShape(lShown, lWanted));

    lWanted.Title = OpaaxString("Another title");
    CHECK_FALSE(IsOtherWindowShape(lShown, lWanted));

    lWanted.Width = 960;
    CHECK(IsOtherWindowShape(lShown, lWanted));

    lWanted        = lShown;
    lWanted.Height = 540;
    CHECK(IsOtherWindowShape(lShown, lWanted));

    lWanted      = lShown;
    lWanted.Mode = EWindowMode::Fullscreen;
    CHECK(IsOtherWindowShape(lShown, lWanted));
}

TEST_CASE("IWindowManager: no window, no display modes")
{
    CHECK(IWindowManager::Null().GetDisplayModes().empty());

    WindowManager lWm;
    CHECK(lWm.GetDisplayModes().empty());
    CHECK_FALSE(lWm.IsFollowingEngineConfig());
}

// =============================================================================
// DisplayMode — the list a resolution menu offers, and the mode fullscreen gets
// =============================================================================

TEST_CASE("SortDisplayModes: smallest first, then by refresh rate; each mode once")
{
    const TDynArray<DisplayMode> lSorted = SortDisplayModes({
        { 1920, 1080, 240 }, { 1280, 720, 60 }, { 1920, 1080, 60 }, { 1280, 720, 60 },
        { 2560, 1440, 144 }, { 1920, 1080, 144 }, { 1280, 1024, 60 } });

    const TDynArray<DisplayMode> lExpected = {
        { 1280, 720, 60 }, { 1280, 1024, 60 }, { 1920, 1080, 60 }, { 1920, 1080, 144 }, { 1920, 1080, 240 },
        { 2560, 1440, 144 } };
    CHECK(lSorted == lExpected);

    CHECK(SortDisplayModes({}).empty());
}

TEST_CASE("ChooseDisplayMode: the size asked at its highest refresh rate, else the closest size")
{
    const TDynArray<DisplayMode> lModes = {
        { 1280, 720, 60 }, { 1920, 1080, 60 }, { 1920, 1080, 240 }, { 1920, 1080, 144 }, { 2560, 1440, 144 } };

    // A 240 Hz screen's own size: fullscreen at 240 Hz, not the first one listed.
    CHECK(ChooseDisplayMode(lModes, 1920, 1080) == DisplayMode{ 1920, 1080, 240 });
    CHECK(ChooseDisplayMode(lModes, 1280, 720) == DisplayMode{ 1280, 720, 60 });

    // A window size the screen does not have: the nearest it has.
    CHECK(ChooseDisplayMode(lModes, 1366, 768) == DisplayMode{ 1280, 720, 60 });
    CHECK(ChooseDisplayMode(lModes, 1800, 1000) == DisplayMode{ 1920, 1080, 240 });
    CHECK(ChooseDisplayMode(lModes, 7680, 4320) == DisplayMode{ 2560, 1440, 144 });

    // A screen that lists nothing: the size asked, at whatever rate it has.
    CHECK(ChooseDisplayMode({}, 1600, 900) == DisplayMode{ 1600, 900, 0 });
}

TEST_CASE("FindScreenAt: the screen holding a window's centre, side by side or above")
{
    const TDynArray<ScreenArea> lScreens = {
        { 0, 0, 1920, 1080 },        // the main screen
        { 1920, 0, 2560, 1440 },     // to its right
        { 0, -1080, 1920, 1080 } };  // above it

    CHECK(FindScreenAt(lScreens, 960, 540) == 0);
    CHECK(FindScreenAt(lScreens, 1919, 1079) == 0);
    CHECK(FindScreenAt(lScreens, 1920, 0) == 1);
    CHECK(FindScreenAt(lScreens, 3000, 1300) == 1);
    CHECK(FindScreenAt(lScreens, 500, -10) == 2);

    // Off every screen (below the main one, or far right): none.
    CHECK(FindScreenAt(lScreens, 960, 1200) == -1);
    CHECK(FindScreenAt(lScreens, 5000, 10) == -1);
    CHECK(FindScreenAt({}, 0, 0) == -1);
}
