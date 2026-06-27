// Suite: IConfigSystem registry. A test-local IConfig (TestConfig) records its Load/Save
// calls (no real disk IO) so we can verify the registry mechanics: file-per-config path
// resolution, same-instance Get, auto-register-on-miss (never null), Save / SaveAll, and the
// null system staying in-memory.
#include <doctest.h>

#include <utility>

#include "Core/Application/Services/IConfigSystem.h"
#include "Core/Application/Services/IPaths.h"
#include "Core/Application/Services/AppServiceLocator.h"
#include "Core/Config/IConfig.h"

using namespace Opaax;

namespace
{
    class TestConfig final : public IConfig
    {
    public:
        OPAAX_CONFIG_TYPE(TestConfig)

        const char* FileName() const override { return "test.config.json"; }

        bool Load(const OpaaxString& InAbsPath) override { m_LoadedPath = InAbsPath; ++m_LoadCount; return true; }
        bool Save(const OpaaxString& InAbsPath) override { m_LoadedPath = InAbsPath; ++m_SaveCount; return true; }
        bool Save()                              override { ++m_SaveCount; return true; }

        const OpaaxString& LoadedPath() const { return m_LoadedPath; }
        int LoadCount() const { return m_LoadCount; }
        int SaveCount() const { return m_SaveCount; }

    protected:
        bool GenerateDefaultConfig(const OpaaxString&) override { return true; }

    private:
        OpaaxString m_LoadedPath;
        int         m_LoadCount = 0;
        int         m_SaveCount = 0;
    };

    ConfigTypeID TestConfig::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ConfigTypeID>(&s_Tag);
    }

    // Minimal IPaths whose only meaningful answer is ConfigsDir().
    class StubPaths final : public IPaths
    {
    public:
        explicit StubPaths(OpaaxString InConfigsDir) : m_ConfigsDir(std::move(InConfigsDir)) {}
        OpaaxString ConfigsDir()   const override { return m_ConfigsDir; }

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
