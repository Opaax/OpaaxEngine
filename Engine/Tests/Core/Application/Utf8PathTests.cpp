// Suite: the I7 boundary — Core/String/OpaaxUtf8.h and the call sites that open files through it.
//
// Every Unicode name here is written with \uXXXX ESCAPES, never literal characters: this file has no
// BOM and the build sets no /utf-8, so MSVC would decode literal bytes using the ANSI code page — the
// exact mechanism under test. An instrument must not share a failure mode with the thing it measures
// ([[L21]]). For the same reason the "did it really work?" checks go through std::filesystem's WIDE
// API, not through the narrow one being tested.
//
// U+65E5 U+672C are outside CP-1252 entirely, so those cases cannot pass by being consistently wrong.
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Core/String/OpaaxUtf8.h"
#include "Core/IO/FileIO.h"
#include "Application/Services/IPaths.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"   // before BinaryResource — completes LoadContext
#include "Engine/Subsystems/Resources/Types/BinaryResource.hpp"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    // The non-ASCII directory name, in both worlds. Same characters, stated twice on purpose:
    // once as UTF-16 for the OS, once as UTF-8 bytes for the engine.
    const wchar_t* const kWideDir = L"\u65E5\u672C_Caf\u00E9";
    inline OpaaxString Utf8Dir() { return OpaaxString("\xE6\x97\xA5\xE6\x9C\xAC" "_Caf" "\xC3\xA9"); }

    // A temp directory whose NAME is non-ASCII — created wide, so what is on disk is unambiguous.
    class ScopedUnicodeDir
    {
    public:
        explicit ScopedUnicodeDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / (std::string("OpaaxUtf8Tests_") + InTag) / kWideDir;

            std::error_code lError;
            fs::remove_all(m_Path.parent_path(), lError);
            fs::create_directories(m_Path, lError);
        }

        ~ScopedUnicodeDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path.parent_path(), lError);
        }

        ScopedUnicodeDir(const ScopedUnicodeDir&)            = delete;
        ScopedUnicodeDir& operator=(const ScopedUnicodeDir&) = delete;

        const fs::path& Wide() const { return m_Path; }

        // The same directory as the engine would carry it: UTF-8 bytes, forward slashes.
        OpaaxString Utf8() const { return Utf8::FromFsPath(m_Path); }

        OpaaxString Utf8File(const char* InLeaf) const
        {
            return Utf8() + OpaaxString("/") + OpaaxString(InLeaf);
        }

    private:
        fs::path m_Path;
    };
}

// =============================================================================
// The primitive
// =============================================================================
TEST_CASE("Utf8: ToFsPath/FromFsPath round-trip a non-ASCII path without touching the ANSI code page")
{
    const OpaaxString lUtf8 = Utf8Dir();
    const fs::path    lPath = Utf8::ToFsPath(lUtf8);

    // The path really holds the Unicode characters — compare against the WIDE literal, which cannot
    // have been produced by a mis-decode of the UTF-8 bytes.
    CHECK(lPath.wstring() == std::wstring(kWideDir));

    // ...and it survives the trip home.
    CHECK(Utf8::FromFsPath(lPath) == lUtf8);
}

TEST_CASE("Utf8: FromFsPath normalises separators to '/' (the engine's path convention)")
{
    // Native separators in, generic separators out — that is the whole job.
    const fs::path    lPath = fs::path(L"C:\\A\\B\\c.txt");
    const OpaaxString lOut  = Utf8::FromFsPath(lPath);

    CHECK(lOut.Find("\\") == -1);
    CHECK(lOut == "C:/A/B/c.txt");
}

TEST_CASE("Utf8: empty in, empty out — every entry point, no crash")
{
    CHECK(Utf8::ToFsPath(OpaaxString()).empty());
    CHECK(Utf8::FromFsPath(fs::path()).IsEmpty());
}

// =============================================================================
// Codepoint decoding — the same header seen from the text-rendering end (⑥ S4).
//
// Every sequence below is written as explicit \x BYTES, not as \uXXXX and never as literal
// characters. The subject IS the byte decoder, so stating the bytes is stating the input; anything
// else would put the source file's encoding between the case and the thing it measures ([[L21]]).
// =============================================================================
TEST_CASE("Utf8::Decode: one, two, three and four byte sequences, each advancing by its own length")
{
    // U+0041 'A' · U+0393 GREEK CAPITAL GAMMA · U+65E5 (CJK, outside CP-1252 entirely) ·
    // U+1F600 GRINNING FACE, which needs the four-byte form.
    const char* lCursor = "\x41" "\xCE\x93" "\xE6\x97\xA5" "\xF0\x9F\x98\x80";
    const char* lStart  = lCursor;

    CHECK(Utf8::Decode(lCursor) == 0x0041u);
    CHECK(lCursor - lStart == 1);

    CHECK(Utf8::Decode(lCursor) == 0x0393u);
    CHECK(lCursor - lStart == 3);

    CHECK(Utf8::Decode(lCursor) == 0x65E5u);
    CHECK(lCursor - lStart == 6);

    CHECK(Utf8::Decode(lCursor) == 0x1F600u);
    CHECK(lCursor - lStart == 10);
}

TEST_CASE("Utf8::Decode: the terminator answers 0 and does NOT advance — the loop condition")
{
    const char* lCursor = "";
    const char* lStart  = lCursor;

    CHECK(Utf8::Decode(lCursor) == 0u);
    CHECK(lCursor == lStart);

    // Asked again it must still answer 0 rather than walking off the end.
    CHECK(Utf8::Decode(lCursor) == 0u);
    CHECK(lCursor == lStart);
}

TEST_CASE("Utf8::Decode: every malformed form answers REPLACEMENT and consumes exactly one byte")
{
    // The property under test is not the value, it is the ADVANCE: a decoder that can stand still
    // turns one bad byte into a hang, which is the only failure mode here that is not survivable.
    struct Malformed { const char* Bytes; const char* What; };

    const Malformed lCases[] =
    {
        { "\x80",         "a stray continuation byte with no lead" },
        { "\xC0\xAF",     "an overlong two-byte '/'" },
        { "\xE0\x80\xAF", "an overlong three-byte '/'" },
        { "\xED\xA0\x80", "a UTF-16 surrogate half (U+D800), encodable and illegal" },
        { "\xF5\x80\x80\x80", "a lead past U+10FFFF" },
        { "\xE6\x97",     "a three-byte sequence truncated by the terminator" },
    };

    for (const Malformed& lCase : lCases)
    {
        CAPTURE(lCase.What);

        const char* lCursor = lCase.Bytes;
        const char* lStart  = lCursor;

        CHECK(Utf8::Decode(lCursor) == Utf8::REPLACEMENT);
        CHECK(lCursor - lStart == 1);
    }
}

TEST_CASE("Utf8::Decode: a mixed-script string walks to its end and terminates")
{
    // "AΓ日" — the shape a text draw actually feeds it.
    const char* lCursor = "\x41" "\xCE\x93" "\xE6\x97\xA5";

    Uint32 lCount = 0;
    while (Utf8::Decode(lCursor) != 0u)
    {
        ++lCount;
        REQUIRE(lCount < 16u);   // a decoder that failed to advance would spin here, not hang the suite
    }

    CHECK(lCount == 3u);
}

// =============================================================================
// The call sites that open files
// =============================================================================
TEST_CASE("FileIO: text round-trips through a non-ASCII directory")
{
    const ScopedUnicodeDir lDir("fileio_text");
    const OpaaxString      lFile = lDir.Utf8File("Settings.json");

    REQUIRE(FileIO::WriteAllText(lFile, OpaaxString("{\"volume\":11}")));

    // The file landed where the engine SAID it would — asked through the wide API.
    CHECK(fs::exists(lDir.Wide() / L"Settings.json"));

    CHECK(FileIO::ReadAllText(lFile) == "{\"volume\":11}");
}

TEST_CASE("FileIO: WriteAllText creates missing parents under a non-ASCII root")
{
    const ScopedUnicodeDir lDir("fileio_parents");
    const OpaaxString      lFile = lDir.Utf8() + OpaaxString("/Nested/Deep/Settings.json");

    REQUIRE(FileIO::WriteAllText(lFile, OpaaxString("x")));
    CHECK(fs::is_directory(lDir.Wide() / L"Nested" / L"Deep"));
}

TEST_CASE("FileIO: ReadAllBytes reads a non-ASCII path and leaves the output alone on failure")
{
    const ScopedUnicodeDir lDir("fileio_bytes");

    {
        std::ofstream lOut(lDir.Wide() / L"blob.bin", std::ios::binary);   // written WIDE
        lOut << "opaax";
    }

    TDynArray<Uint8> lBytes;
    REQUIRE(FileIO::ReadAllBytes(lDir.Utf8File("blob.bin"), lBytes));
    CHECK(lBytes.size() == 5);

    // A rejected read must not disturb what the caller already held.
    CHECK_FALSE(FileIO::ReadAllBytes(lDir.Utf8File("missing.bin"), lBytes));
    CHECK(lBytes.size() == 5);
}

TEST_CASE("FileIO: a missing file and an empty path are ordinary answers, never throws")
{
    const ScopedUnicodeDir lDir("fileio_missing");

    CHECK(FileIO::ReadAllText(lDir.Utf8File("nope.txt")).IsEmpty());
    CHECK(FileIO::ReadAllText(OpaaxString()).IsEmpty());
    CHECK_FALSE(FileIO::WriteAllText(OpaaxString(), OpaaxString("x")));

    TDynArray<Uint8> lBytes;
    CHECK_FALSE(FileIO::ReadAllBytes(OpaaxString(), lBytes));
}

TEST_CASE("BinaryResource: loads a file from a non-ASCII directory")
{
    const ScopedUnicodeDir lDir("binary");

    {
        std::ofstream lOut(lDir.Wide() / L"blob.bin", std::ios::binary);   // written WIDE
        lOut << "opaax";
    }

    // BinaryResource::Load ignores the context; it just has to be a live one.
    ResourceManager         lManager;
    ResourceDependencyGraph lDeps;
    LoadContext             lCtx(lManager, lDeps);

    const std::optional<BinaryResource> lRes =
        BinaryResource::Load(lDir.Utf8File("blob.bin").CStr(), lCtx);

    REQUIRE(lRes.has_value());
    CHECK(lRes->Bytes.size() == 5);
}

// NOTE: the shader path's encoding coverage now lives in the FileIO cases above — ShaderSource no
// longer opens files at all (the host reads the text and passes it in), so there is nothing
// encoding-sensitive left in it to test here.

// =============================================================================
// Path composition
// =============================================================================
TEST_CASE("ResolveProjectLayout: a non-ASCII workspace survives resolution intact")
{
    const OpaaxString lWorkspace = OpaaxString("W:/") + Utf8Dir();

    const ProjectLayout lLayout = ResolveProjectLayout(
        OpaaxString("W:/build/bin/Game.exe"),
        lWorkspace,
        OpaaxString());

    CHECK(lLayout.WorkspaceRoot == lWorkspace);
    CHECK(lLayout.EngineRoot    == lWorkspace + OpaaxString("/Engine"));

    // The project root is <workspace>/<AppName> — the exe stem must not have been narrowed either.
    CHECK(lLayout.ProjectRoot   == lWorkspace + OpaaxString("/Game"));
    CHECK(lLayout.ProjectFile   == lWorkspace + OpaaxString("/Game/Game.opaaxproj"));
    CHECK(lLayout.SaveDir       == lWorkspace + OpaaxString("/Game/Save"));
}
