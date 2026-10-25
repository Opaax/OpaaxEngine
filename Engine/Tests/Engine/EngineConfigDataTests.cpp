// Suite: EngineConfigData through the generic codec (the same macro as components).
// Missing keys keep defaults, malformed input keeps defaults, and the round trip is exact.
// The codec throws; TConfig::Load catches. And what TConfig gives every config: reset, the values the
// Config panel draws, change notification.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/String/CString.h"

#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Config/TConfig.h"
#include "Renderer/Config/RendererConfigData.h"

using namespace Opaax;

namespace
{
    using Codec = TConfigCodec<EngineConfigData>;

    EngineConfigData Parse(const char* InText) { return Codec::FromText(OpaaxString(InText)); }

    // A config over the same data, so Load's behaviour can be exercised without the config system.
    class ProbeConfig final : public TConfig<EngineConfigData>
    {
        OPAAX_OBJECT(ProbeConfig, Config)

    public:
        const char* FileName() const override { return "probe.config"; }
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
    const TestTempDir lDir("ConfigTolerance");
    const OpaaxString lPath = lDir.Sub("Probe.config");
    FileIO::WriteAllText(lPath, OpaaxString("{ not json at all"));

    ProbeConfig lProbe;
    REQUIRE_FALSE(lProbe.Load(lPath));

    // Defaults intact — a half-applied config would be worse than none.
    CHECK(lProbe.GetData().Window.Width == 1280u);
    CHECK(lProbe.GetData().Render.Backend == EBackend::OpenGL);
}

TEST_CASE("TConfig::Load: a file it CAN parse answers true")
{
    const TestTempDir lDir("ConfigGood");
    const OpaaxString lPath = lDir.Sub("Probe.config");
    FileIO::WriteAllText(lPath, OpaaxString(R"({"Window":{"Width":900}})"));

    ProbeConfig lProbe;
    CHECK(lProbe.Load(lPath));
    CHECK(lProbe.GetData().Window.Width == 900u);
}

TEST_CASE("TConfig::Load: a misspelled ENUMERATOR is refused like any other unreadable value")
{
    // A typo in an enum value is refused like any unreadable value (ConfigSystem warns).
    const TestTempDir lDir("ConfigBadEnum");
    const OpaaxString lPath = lDir.Sub("Probe.config");
    FileIO::WriteAllText(lPath,
                         OpaaxString(R"({"Window":{"Width":900,"Mode":"Borderles"}})"));

    ProbeConfig lProbe;
    CHECK_FALSE(lProbe.Load(lPath));

    // Nothing half-applied: the good Width beside the bad Mode did not land either.
    CHECK(lProbe.GetData().Window.Width == 1280u);
    CHECK(lProbe.GetData().Window.Mode  == EWindowMode::Windowed);
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
}

TEST_CASE("Config::NotifyChanged: every subscriber is called exactly once")
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

TEST_CASE("Config::OnChanged: after RemoveAll(owner) the subscriber is no longer called")
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
    const TestTempDir lDir("ConfigNotifyGood");
    const OpaaxString lPath = lDir.Sub("Probe.config");
    FileIO::WriteAllText(lPath, OpaaxString(R"({"Window":{"Width":900}})"));

    ProbeConfig   lProbe;
    ChangeCounter lMember;
    lProbe.OnChanged().AddMember(&lMember, &ChangeCounter::OnChanged);

    REQUIRE(lProbe.Load(lPath));

    CHECK(lMember.Calls == 1);
    CHECK(lProbe.GetData().Window.Width == 900u);   // the values ARE in place when it fires
}

TEST_CASE("TConfig::Load: an unreadable file resets to defaults, and that is a change too")
{
    const TestTempDir lDir("ConfigNotifyBad");
    const OpaaxString lPath = lDir.Sub("Probe.config");
    FileIO::WriteAllText(lPath, OpaaxString("{ not json at all"));

    ProbeConfig lProbe;
    lProbe.GetData().Window.Width = 900u;   // a value the reset will take away

    ChangeCounter lMember;
    lProbe.OnChanged().AddMember(&lMember, &ChangeCounter::OnChanged);

    REQUIRE_FALSE(lProbe.Load(lPath));

    CHECK(lMember.Calls == 1);
    CHECK(lProbe.GetData().Window.Width == 1280u);
}

TEST_CASE("TConfig::Load: a MISSING file writes the defaults and does NOT notify")
{
    // Nothing in memory changed — the file was generated FROM memory.
    const TestTempDir lDir("ConfigNotifyMissing");
    const OpaaxString lPath = lDir.Sub("Probe.config");

    ProbeConfig   lProbe;
    ChangeCounter lMember;
    lProbe.OnChanged().AddMember(&lMember, &ChangeCounter::OnChanged);

    REQUIRE(lProbe.Load(lPath));

    CHECK(lMember.Calls == 0);
    CHECK(FileSystem::Exists(lPath));   // proves the branch taken was the generate one
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
            if (CString::Equals(InProperty.Name, InName))
            {
                lFlags = InProperty.Meta.Flags;
                bFound = true;
            }
        };
        TupleApply([&](const auto&... InEach) { (lVisit(InEach), ...); }, InProperties);

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

// =============================================================================
// What every config gets from TConfig
// =============================================================================

namespace
{
    /** Records the groups and fields a walk meets, at any depth. */
    class ValueNames final : public IPropertyVisitor
    {
    public:
        TDynArray<OpaaxString> Groups;
        Uint32                 Fields = 0;

        void Visit(const char*, bool&, const PropertyMeta&) override          { ++Fields; }
        void Visit(const char*, Int16&, const PropertyMeta&) override         { ++Fields; }
        void Visit(const char*, Int32&, const PropertyMeta&) override         { ++Fields; }
        void Visit(const char*, Uint32&, const PropertyMeta&) override        { ++Fields; }
        void Visit(const char*, float&, const PropertyMeta&) override         { ++Fields; }
        void Visit(const char*, Vector2F&, const PropertyMeta&) override      { ++Fields; }
        void Visit(const char*, Vector3F&, const PropertyMeta&) override      { ++Fields; }
        void Visit(const char*, Vector4F&, const PropertyMeta&) override      { ++Fields; }
        void Visit(const char*, LinearColor&, const PropertyMeta&) override   { ++Fields; }
        void Visit(const char*, OpaaxString&, const PropertyMeta&) override   { ++Fields; }
        void Visit(const char*, OpaaxStringID&, const PropertyMeta&) override { ++Fields; }
        void VisitEnum(const char*, const char* const*, Uint32, Uint32&, const PropertyMeta&) override { ++Fields; }
        void VisitResourcePath(const char*, OpaaxString&, Uint32, const PropertyMeta&) override { ++Fields; }
        void VisitAssetRef(const char*, OpaaxString&, const ObjectClass&, const PropertyMeta&) override { ++Fields; }
        void VisitObjectRef(const char*, ObjectHandle&, const ObjectClass&, const PropertyMeta&) override { ++Fields; }
        void VisitUnsupported(const char*, OpaaxStringView) override { ++Fields; }

        bool BeginGroup(const char* InName, const PropertyMeta&) override
        {
            Groups.emplace_back(InName);
            return true;
        }

        void EndGroup() override {}

        bool HasGroup(const char* InName) const
        {
            for (const OpaaxString& lGroup : Groups)
            {
                if (lGroup == InName) { return true; }
            }
            return false;
        }
    };

    /** Values with no property list: the editor shows their text. */
    struct PlainData
    {
        Int32 Value = 3;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PlainData, Value)
    };

    class PlainConfig final : public TConfig<PlainData>
    {
        OPAAX_OBJECT(PlainConfig, Config)

    public:
        const char* FileName() const override { return "Plain.config"; }
    };
}

TEST_CASE("TConfig: ResetToDefaults puts every value back, without saving or announcing it")
{
    const TestTempDir lDir("ConfigReset");
    const OpaaxString lPath = lDir.Sub("Probe.config");
    FileIO::WriteAllText(lPath, OpaaxString(R"({"Window":{"Width":900},"Render":{"bInterpolation":false}})"));

    ProbeConfig lProbe;
    REQUIRE(lProbe.Load(lPath));
    REQUIRE(lProbe.GetData().Window.Width == 900u);

    ChangeCounter lMember;
    lProbe.OnChanged().AddMember(&lMember, &ChangeCounter::OnChanged);

    lProbe.ResetToDefaults();

    CHECK(lProbe.GetData().Window.Width == 1280u);
    CHECK(lProbe.GetData().Render.bInterpolation);
    CHECK(lProbe.ToText() == Codec::ToText(EngineConfigData{}));

    CHECK(lMember.Calls == 0);                                                    // the panel announces it
    CHECK(FileIO::ReadAllText(lPath).Contains("900"));                            // the file is untouched
}

TEST_CASE("TConfig: VisitValues walks the data's properties, which the Config panel draws")
{
    ProbeConfig lProbe;

    ValueNames lNames;
    CHECK(lProbe.VisitValues(lNames));
    CHECK(lNames.HasGroup("Window"));
    CHECK(lNames.HasGroup("Render"));
    CHECK(lNames.Fields > 0);

    // Values without a property list are not walked: the panel shows the text instead.
    PlainConfig lPlain;
    ValueNames  lNone;
    CHECK_FALSE(lPlain.VisitValues(lNone));
    CHECK(lNone.Fields == 0);
    CHECK(lPlain.ToText().Contains("Value"));
}

TEST_CASE("Config: a config is an Object class under Config, named after its file by NameFromFile")
{
    CHECK(ProbeConfig::StaticClass().IsChildOf(Config::StaticClass()));
    CHECK(Config::StaticClass().IsAbstract());
    CHECK_FALSE(ProbeConfig::StaticClass().IsAbstract());

    // Only the last extension goes: "test.config.json" keeps its inner dot.
    CHECK(Config::NameFromFile("Engine.config") == OPAAX_ID("Engine"));
    CHECK(Config::NameFromFile("test.config.json") == OPAAX_ID("test.config"));
}
