// Suite: IConfigSystem registry. A test-local Config class (TestConfig) records its Load/Save
// calls (no real disk IO) so we can verify the registry mechanics: file-per-config path
// resolution, same-instance Get, auto-register-on-miss (never null), configs named after their
// files, registering every class of a registry, Save / SaveAll, and the null system staying in-memory.
#include <doctest.h>

#include "Core/OpaaxTypes.h"

#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/AppServiceLocator.h"
#include "Engine/Config/Config.h"
#include "Object/ObjectClassRegistry.h"

using namespace Opaax;

namespace
{
    class TestConfig final : public Config
    {
        OPAAX_OBJECT(TestConfig, Config)

    public:
        const char* FileName() const override { return "test.config.json"; }

        bool Load(const OpaaxString& InAbsPath) override { m_LoadedPath = InAbsPath; ++m_LoadCount; return true; }
        bool Save(const OpaaxString& InAbsPath) override { m_LoadedPath = InAbsPath; ++m_SaveCount; return true; }
        bool Save()                              override { ++m_SaveCount; return true; }

        OpaaxString ToText() const override { return OpaaxString("test-text"); }

        bool VisitValues(IPropertyVisitor&) override { return false; }
        void ResetToDefaults() override {}
        bool MergeText(const OpaaxString&, OpaaxString&) override { return false; }

        const OpaaxString& LoadedPath() const { return m_LoadedPath; }
        int LoadCount() const { return m_LoadCount; }
        int SaveCount() const { return m_SaveCount; }

    private:
        OpaaxString m_LoadedPath;
        int         m_LoadCount = 0;
        int         m_SaveCount = 0;
    };

    // A second class, so the ORDER of the registry is observable at all — one config cannot tell
    // "registration order" from "whatever the container did".
    class OtherTestConfig final : public Config
    {
        OPAAX_OBJECT(OtherTestConfig, Config)

    public:
        const char* FileName() const override { return "Other.config"; }

        bool Load(const OpaaxString&) override { return true; }
        bool Save(const OpaaxString&) override { return true; }
        bool Save()                    override { return true; }

        OpaaxString ToText() const override { return OpaaxString("other-text"); }

        bool VisitValues(IPropertyVisitor&) override { return false; }
        void ResetToDefaults() override {}
        bool MergeText(const OpaaxString&, OpaaxString&) override { return false; }
    };

    // A base a game could share between its configs: never made.
    class AbstractTestConfig : public Config
    {
        OPAAX_ABSTRACT_OBJECT(AbstractTestConfig, Config)
    };

    // Any other object class.
    class NotAConfig final : public Object
    {
        OPAAX_OBJECT(NotAConfig, Object)
    };

    // What a game's registry holds: its configs among other classes.
    struct GameClasses final : ObjectClassRegistry
    {
        GameClasses()
        {
            Register<NotAConfig>();
            Register<Config>();
            Register<AbstractTestConfig>();
            Register<OtherTestConfig>();
            Register<TestConfig>();
        }
    };

    // Minimal IPaths whose only meaningful answer is ConfigsDir().
    class StubPaths final : public IPaths
    {
    public:
        explicit StubPaths(OpaaxString InConfigsDir) : m_ConfigsDir(Move(InConfigsDir)) {}
        OpaaxString ConfigsDir()   const override { return m_ConfigsDir; }
        void        LogPaths()     const override {}   // stub

        OpaaxString WorkspaceRoot() const override { return OpaaxString(); }
        OpaaxString EngineRoot()    const override { return OpaaxString(); }
        OpaaxString ProjectRoot()   const override { return OpaaxString(); }
        OpaaxString ProjectFile()   const override { return OpaaxString(); }
        OpaaxString AssetsDir()     const override { return OpaaxString(); }
        OpaaxString SourceDir()     const override { return OpaaxString(); }
        OpaaxString SaveDir()       const override { return OpaaxString(); }
        OpaaxString TempDir()       const override { return OpaaxString(); }
        OpaaxString EngineToAbsolute(const OpaaxString&)  const override { return OpaaxString(); }
        OpaaxString ProjectToAbsolute(const OpaaxString&) const override { return OpaaxString(); }
        OpaaxString AssetToAbsolute(const OpaaxString&)   const override { return OpaaxString(); }
        OpaaxString AbsoluteToAsset(const OpaaxString&)   const override { return OpaaxString(); }
    private:
        OpaaxString m_ConfigsDir;
    };
}

TEST_CASE("ConfigSystem: Register loads from ConfigsDir/FileName; Get returns the same instance")
{
    StubPaths lPaths(OpaaxString("W:/proj/Configs"));
    ConfigSystem lConfig(lPaths);

    TestConfig& lA = lConfig.Register<TestConfig>();
    CHECK(lA.LoadCount() == 1);
    CHECK(lA.LoadedPath() == "W:/proj/Configs/test.config.json"); // file-per-config

    CHECK(&lConfig.Get<TestConfig>() == &lA);  // same instance
    CHECK(lA.LoadCount() == 1);                // Get did not reload
}

TEST_CASE("ConfigSystem: Get auto-registers a never-registered config (never null)")
{
    StubPaths lPaths(OpaaxString("W:/proj/Configs"));
    ConfigSystem lConfig(lPaths);

    TestConfig& lA = lConfig.Get<TestConfig>(); // never Register'd
    CHECK(lA.LoadCount() == 1);                 // auto-registered + loaded
}

TEST_CASE("ConfigSystem: Save<T> and SaveAll persist the config")
{
    StubPaths lPaths(OpaaxString("W:/proj/Configs"));
    ConfigSystem lConfig(lPaths);
    TestConfig& lA = lConfig.Register<TestConfig>();

    CHECK(lConfig.Save<TestConfig>());
    CHECK(lA.SaveCount() == 1);

    lConfig.SaveAll();
    CHECK(lA.SaveCount() == 2);
}

TEST_CASE("ConfigSystem: GetConfigs is REGISTRATION order, and FindConfig resolves by class")
{
    StubPaths lPaths(OpaaxString("W:/proj/Configs"));
    ConfigSystem lConfig(lPaths);

    // Registered second on purpose: the editor's Config panel lists this array, so the order has to
    // be the one the caller registered in and not the one a hash bucket produced.
    Config& lFirst  = lConfig.Register<TestConfig>();
    Config& lSecond = lConfig.Register<OtherTestConfig>();

    REQUIRE(lConfig.GetConfigs().size() == 2);
    CHECK(lConfig.GetConfigs()[0].Get() == &lFirst);
    CHECK(lConfig.GetConfigs()[1].Get() == &lSecond);

    CHECK(lConfig.FindConfig(TestConfig::StaticClass())      == &lFirst);
    CHECK(lConfig.FindConfig(OtherTestConfig::StaticClass()) == &lSecond);
    CHECK(lConfig.FindConfig(AbstractTestConfig::StaticClass()) == nullptr);   // never registered

    // Re-registering is a lookup, not a second entry.
    lConfig.Get<TestConfig>();
    CHECK(lConfig.GetConfigs().size() == 2);
}

TEST_CASE("ConfigSystem: a config is named after its file's stem, ToText comes from the config")
{
    StubPaths lPaths(OpaaxString("W:/proj/Configs"));
    ConfigSystem lConfig(lPaths);

    // Only the LAST extension is stripped — "test.config.json" keeps its inner dot.
    CHECK(lConfig.Register<TestConfig>().GetName()       == OPAAX_ID("test.config"));
    CHECK(lConfig.Register<OtherTestConfig>().GetName()  == OPAAX_ID("Other"));

    CHECK(lConfig.Get<OtherTestConfig>().ToText() == "other-text");
}

TEST_CASE("ConfigSystem: RegisterClasses makes the config of every concrete class not made yet, in order")
{
    StubPaths lPaths(OpaaxString("W:/proj/Configs"));
    ConfigSystem lConfig(lPaths);

    // Read by code before the engine started: kept, not made twice, and still first.
    TestConfig& lEarly = lConfig.Get<TestConfig>();

    const GameClasses lClasses;
    lConfig.RegisterClasses(lClasses);

    // The abstract base and the non-config classes are skipped.
    REQUIRE(lConfig.GetConfigs().size() == 2);
    CHECK(lConfig.GetConfigs()[0].Get() == &lEarly);
    CHECK(&lConfig.GetConfigs()[1]->GetClass() == &OtherTestConfig::StaticClass());
    CHECK(lEarly.LoadCount() == 1);

    // Registered and loaded like any other: Get finds it.
    CHECK(lConfig.GetConfigs()[1]->GetName() == OPAAX_ID("Other"));
    CHECK(&lConfig.Get<OtherTestConfig>() == lConfig.GetConfigs()[1].Get());

    // Running it again changes nothing.
    lConfig.RegisterClasses(lClasses);
    CHECK(lConfig.GetConfigs().size() == 2);
}

TEST_CASE("IConfigSystem: the null system stays in-memory (no disk) and is never null")
{
    AppServiceLocator lLocator;
    IConfigSystem& lSys = lLocator.Get<IConfigSystem>(); // unprovided -> NullConfigSystem
    CHECK(lSys.IsNull());

    TestConfig& lA = lSys.Get<TestConfig>();
    CHECK(lA.LoadCount() == 0);              // null system does NOT load from disk
    CHECK_FALSE(lSys.Save<TestConfig>());
    CHECK(&lSys == &IConfigSystem::Null());
}
