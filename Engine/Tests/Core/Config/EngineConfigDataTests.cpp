// Suite: EngineConfigData through the generic codec (the same macro as components).
// Missing keys keep defaults, malformed input keeps defaults, and the round trip is exact.
// The codec throws; TConfig::Load catches.
#include <doctest.h>

#include <cstring>
#include <filesystem>

#include "Core/Config/TConfig.hpp"
#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxUtf8.h"
#include "Engine/Config/EngineConfigData.h"
#include "Renderer/Config/RendererConfigData.h"

using namespace Opaax;

namespace fs = std::filesystem;

namespace
{
    using Codec = TConfigCodec<EngineConfigData>;

    EngineConfigData Parse(const char* InText) { return Codec::FromText(OpaaxString(InText)); }

    // A config over the same data, so Load's behaviour can be exercised without the registry.
    // Its id is fixed rather than stamped with OPAAX_CONFIG_TYPE: nothing registers it.
    class ProbeConfig final : public TConfig<EngineConfigData>
    {
    public:
        const char*  FileName() const override { return "probe.config"; }
        ConfigTypeID GetConfigTypeID() const noexcept override { return 0; }
    };
}

TEST_CASE("EngineConfigData: full schema reads every group")
{
    const EngineConfigData lData = Parse(R"({
        "Window":  {"Title":"My","Width":1920,"Height":1080,"Mode":"Borderless"},
        "Render":  {"Backend":"Vulkan"}
    })");

    CHECK(lData.Window.Title  == "My");
    CHECK(lData.Window.Width  == 1920u);
    CHECK(lData.Window.Height == 1080u);
    // Same json text as when this was a string field — it parses into the enumerator now.
    CHECK(lData.Window.Mode   == EWindowMode::Borderless);
    CHECK(lData.Render.Backend == EBackend::Vulkan);
}

TEST_CASE("EngineConfigData: an unknown group is IGNORED, not a failure")
{
    // A config with groups that no longer exist still opens (unknown keys are skipped).
    const EngineConfigData lData = Parse(R"({
        "Window":  {"Width":1920},
        "Assets":  {"EngineRoot":"E/A"},
        "Physics": {"Backend":"Box2D"}
    })");

    CHECK(lData.Window.Width == 1920u);
}

TEST_CASE("EngineConfigData: missing fields keep their defaults, at every depth")
{
    // One key, two levels down, and nothing else — the shape of a config written before the rest
    // of the schema existed. _WITH_DEFAULT is what makes this a read rather than a throw.
    const EngineConfigData lData = Parse(R"({"Window":{"Width":800}})");

    CHECK(lData.Window.Width  == 800u);       // overridden
    CHECK(lData.Window.Height == 720u);       // sibling default
    CHECK(lData.Window.Mode   == EWindowMode::Windowed);
    CHECK(lData.Render.Backend == EBackend::OpenGL);  // absent GROUP defaults whole
}

TEST_CASE("EngineConfigData: round trip through the generic codec is exact")
{
    EngineConfigData lIn;
    lIn.Window.Width   = 1600;
    lIn.Window.Mode    = EWindowMode::Fullscreen;
    lIn.Render.Backend = EBackend::Vulkan;

    const EngineConfigData lOut = Codec::FromText(Codec::ToText(lIn));

    CHECK(lOut.Window.Width == 1600u);
    CHECK(lOut.Window.Mode  == EWindowMode::Fullscreen);
    CHECK(lOut.Render.Backend == EBackend::Vulkan);
}

TEST_CASE("EngineConfigData: the file NESTS because the C++ nests")
{
    const nlohmann::json lJson = nlohmann::json::parse(Codec::ToText(EngineConfigData{}).CStr());

    REQUIRE(lJson.contains("Window"));
    CHECK(lJson["Window"].is_object());
    CHECK(lJson["Window"].contains("Title"));
    CHECK(lJson["Render"].is_object());
    CHECK(lJson["Render"].contains("Backend"));
}

// =============================================================================
// Tolerance — in TConfig::Load
// =============================================================================
TEST_CASE("TConfig::Load: a file it cannot parse keeps the defaults and answers FALSE")
{
    // The CODEC is allowed to throw — that is what lets it be one line instead of a hundred.
    CHECK_THROWS(Parse("{ not json"));
    CHECK_THROWS(Parse(R"({"Window":{"Width":"not a number"}})"));

    // Load turns it into policy: an unreadable config must not stop the boot (ConfigSystem warns).
    const fs::path lPath = fs::temp_directory_path() / "OpaaxConfigTolerance.config";
    FileIO::WriteAllText(OpaaxString(lPath.string().c_str()), OpaaxString("{ not json at all"));

    ProbeConfig lProbe;
    REQUIRE_FALSE(lProbe.Load(OpaaxString(lPath.string().c_str())));

    // Defaults intact — a half-applied config would be worse than none.
    CHECK(lProbe.GetData().Window.Width == 1280u);
    CHECK(lProbe.GetData().Render.Backend == EBackend::OpenGL);

    fs::remove(lPath);
}

TEST_CASE("TConfig::Load: a file it CAN parse answers true")
{
    const fs::path lPath = fs::temp_directory_path() / "OpaaxConfigGood.config";
    FileIO::WriteAllText(OpaaxString(lPath.string().c_str()), OpaaxString(R"({"Window":{"Width":900}})"));

    ProbeConfig lProbe;
    CHECK(lProbe.Load(OpaaxString(lPath.string().c_str())));
    CHECK(lProbe.GetData().Window.Width == 900u);

    fs::remove(lPath);
}

TEST_CASE("TConfig::Load: a misspelled ENUMERATOR is refused like any other unreadable value")
{
    // A typo in an enum value is refused like any unreadable value (ConfigSystem warns).
    const fs::path lPath = fs::temp_directory_path() / "OpaaxConfigBadEnum.config";
    FileIO::WriteAllText(OpaaxString(lPath.string().c_str()),
                         OpaaxString(R"({"Window":{"Width":900,"Mode":"Borderles"}})"));

    ProbeConfig lProbe;
    CHECK_FALSE(lProbe.Load(OpaaxString(lPath.string().c_str())));

    // Nothing half-applied: the good Width beside the bad Mode did not land either.
    CHECK(lProbe.GetData().Window.Width == 1280u);
    CHECK(lProbe.GetData().Window.Mode  == EWindowMode::Windowed);

    fs::remove(lPath);
}

// =============================================================================
// Change notification
// =============================================================================

namespace
{
    struct ChangeCounter
    {
        int Calls = 0;
        void OnChanged() { ++Calls; }
    };

    OpaaxString TempConfigPath(const char* InName)
    {
        return Utf8::FromFsPath(fs::temp_directory_path() / InName);
    }
}

TEST_CASE("IConfig::NotifyChanged: every subscriber is called exactly once")
{
    ProbeConfig   lProbe;
    ChangeCounter lMember;
    int           lLambdaCalls = 0;

    lProbe.OnChanged().AddMember(&lMember, &ChangeCounter::OnChanged);
    lProbe.OnChanged().Add([&lLambdaCalls] { ++lLambdaCalls; });

    lProbe.NotifyChanged();

    CHECK(lMember.Calls == 1);
    CHECK(lLambdaCalls  == 1);
}

TEST_CASE("IConfig::OnChanged: after RemoveAll(owner) the subscriber is no longer called")
{
    // The unsubscribe every subsystem must do in Shutdown (the config outlives it).
    ProbeConfig   lProbe;
    ChangeCounter lMember;

    lProbe.OnChanged().AddMember(&lMember, &ChangeCounter::OnChanged);
    lProbe.OnChanged().RemoveAll(&lMember);

    lProbe.NotifyChanged();

    CHECK(lMember.Calls == 0);
    CHECK_FALSE(lProbe.OnChanged().IsBound());
}

TEST_CASE("TConfig::Load: a successful read notifies once")
{
    const OpaaxString lPath = TempConfigPath("OpaaxConfigNotifyGood.config");
    FileIO::WriteAllText(lPath, OpaaxString(R"({"Window":{"Width":900}})"));

    ProbeConfig   lProbe;
    ChangeCounter lMember;
    lProbe.OnChanged().AddMember(&lMember, &ChangeCounter::OnChanged);

    REQUIRE(lProbe.Load(lPath));

    CHECK(lMember.Calls == 1);
    CHECK(lProbe.GetData().Window.Width == 900u);   // the values ARE in place when it fires

    fs::remove(Utf8::ToFsPath(lPath));
}

TEST_CASE("TConfig::Load: an unreadable file resets to defaults, and that is a change too")
{
    const OpaaxString lPath = TempConfigPath("OpaaxConfigNotifyBad.config");
    FileIO::WriteAllText(lPath, OpaaxString("{ not json at all"));

    ProbeConfig lProbe;
    lProbe.GetData().Window.Width = 900u;   // a value the reset will take away

    ChangeCounter lMember;
    lProbe.OnChanged().AddMember(&lMember, &ChangeCounter::OnChanged);

    REQUIRE_FALSE(lProbe.Load(lPath));

    CHECK(lMember.Calls == 1);
    CHECK(lProbe.GetData().Window.Width == 1280u);

    fs::remove(Utf8::ToFsPath(lPath));
}

TEST_CASE("TConfig::Load: a MISSING file writes the defaults and does NOT notify")
{
    // Nothing in memory changed — the file was generated FROM memory.
    const OpaaxString lPath = TempConfigPath("OpaaxConfigNotifyMissing.config");
    fs::remove(Utf8::ToFsPath(lPath));

    ProbeConfig   lProbe;
    ChangeCounter lMember;
    lProbe.OnChanged().AddMember(&lMember, &ChangeCounter::OnChanged);

    REQUIRE(lProbe.Load(lPath));

    CHECK(lMember.Calls == 0);
    CHECK(fs::exists(Utf8::ToFsPath(lPath)));   // proves the branch taken was the generate one

    fs::remove(Utf8::ToFsPath(lPath));
}

// =============================================================================
// NeedRestart is set on the fields read at startup, not on the ones applied live.
// =============================================================================

namespace
{
    /** A property's flags, found by NAME — an index would silently follow a reorder. */
    template<typename TProperties>
    EPropertyFlags FlagsOf(const TProperties& InProperties, const char* InName)
    {
        EPropertyFlags lFlags = EPropertyFlags::None;
        bool           bFound = false;

        const auto lVisit = [&](const auto& InProperty)
        {
            if (std::strcmp(InProperty.Name, InName) == 0)
            {
                lFlags = InProperty.Meta.Flags;
                bFound = true;
            }
        };
        std::apply([&](const auto&... InEach) { (lVisit(InEach), ...); }, InProperties);

        REQUIRE_MESSAGE(bFound, "no property named " << InName);
        return lFlags;
    }
}

TEST_CASE("EngineConfigData: Render.Backend needs a restart, Render.bInterpolation does not")
{
    const auto lRender = RenderSettings::GetProperties();
    CHECK(HasFlag(FlagsOf(lRender, "Backend"), EPropertyFlags::NeedRestart));
    CHECK_FALSE(HasFlag(FlagsOf(lRender, "bInterpolation"), EPropertyFlags::NeedRestart));

    // The GROUP no longer claims it for both fields; the all-boot groups still do.
    const auto lEngine = EngineConfigData::GetProperties();
    CHECK_FALSE(HasFlag(FlagsOf(lEngine, "Render"), EPropertyFlags::NeedRestart));
    CHECK(HasFlag(FlagsOf(lEngine, "Window"), EPropertyFlags::NeedRestart));
}

TEST_CASE("RendererConfigData: ClearColor is live, the batch limits need a restart")
{
    const auto lRenderer = RendererConfigData::GetProperties();
    CHECK_FALSE(HasFlag(FlagsOf(lRenderer, "ClearColor"), EPropertyFlags::NeedRestart));
    CHECK(HasFlag(FlagsOf(lRenderer, "MaxQuadsPerBatch"), EPropertyFlags::NeedRestart));
    CHECK(HasFlag(FlagsOf(lRenderer, "MaxTextureSlots"), EPropertyFlags::NeedRestart));
}
