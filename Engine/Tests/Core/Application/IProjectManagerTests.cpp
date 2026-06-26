// Suite: IProjectManager — identity read from the .opaaxproj. The parsing is a PURE,
// tolerant function (ParseProjectIdentity) driven with JSON strings; the service ctor's
// file read is thin glue. NullPaths gives an empty identity (no file) through the locator.
#include <doctest.h>

#include "Core/Application/Services/IProjectManager.h"
#include "Core/Application/Services/IPaths.h"
#include "Core/Application/Services/AppServiceLocator.h"

using namespace Opaax;

// =============================================================================
// Pure parser
// =============================================================================
TEST_CASE("ParseProjectIdentity: full schema reads every field")
{
    const ProjectIdentity lId = ParseProjectIdentity(OpaaxString(
        R"({"name":"MyGame","id":"a1b2","engineVersion":"0.4","startupScene":"Scenes/Main.opaaxscene"})"));

    CHECK(lId.Name          == "MyGame");
    CHECK(lId.Id            == "a1b2");
    CHECK(lId.EngineVersion == "0.4");
    CHECK(lId.StartupScene  == "Scenes/Main.opaaxscene");
}

TEST_CASE("ParseProjectIdentity: legacy 'defaultScene' feeds StartupScene")
{
    const ProjectIdentity lId = ParseProjectIdentity(OpaaxString(
        R"({"name":"Sandbox","defaultScene":"Scenes/Boot.opaaxscene"})"));

    CHECK(lId.Name         == "Sandbox");
    CHECK(lId.StartupScene == "Scenes/Boot.opaaxscene");
    CHECK(lId.Id.IsEmpty());
    CHECK(lId.EngineVersion.IsEmpty());
}

TEST_CASE("ParseProjectIdentity: missing fields default to empty")
{
    const ProjectIdentity lId = ParseProjectIdentity(OpaaxString(R"({"name":"X"})"));

    CHECK(lId.Name == "X");
    CHECK(lId.Id.IsEmpty());
    CHECK(lId.EngineVersion.IsEmpty());
    CHECK(lId.StartupScene.IsEmpty());
}

TEST_CASE("ParseProjectIdentity: malformed JSON yields an empty identity (no throw)")
{
    const ProjectIdentity lId = ParseProjectIdentity(OpaaxString("{ this is not json"));

    CHECK(lId.Name.IsEmpty());
    CHECK(lId.StartupScene.IsEmpty());
}

// =============================================================================
// Service / null object
// =============================================================================
TEST_CASE("IProjectManager: unprovided resolves to the null object")
{
    AppServiceLocator lLocator;
    IProjectManager& lResolved = lLocator.Get<IProjectManager>();

    CHECK(lResolved.IsNull());
    CHECK(lResolved.Name().IsEmpty());
    CHECK(&lResolved == &IProjectManager::Null());
}

TEST_CASE("IProjectManager: provided ProjectManager constructs (empty identity over NullPaths)")
{
    AppServiceLocator lLocator;
    // NullPaths.ProjectFile() == "" -> no file -> empty identity, but a real (non-null) service.
    IProjectManager& lPm = lLocator.Provide<IProjectManager, ProjectManager>(IPaths::Null());

    CHECK_FALSE(lPm.IsNull());
    CHECK(lPm.Name().IsEmpty());
}
