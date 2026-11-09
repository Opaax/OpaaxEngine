// Suite: DynamicLibrary (Platform/DynamicLibrary.h), a library opened while the program runs. Checked
// on the system's own C library, which every machine has, and on files that are not libraries.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Platform/DynamicLibrary.h"

using namespace Opaax;

namespace
{
    // A library every machine of the platform has, and a function it exports.
#if defined(OPAAX_PLATFORM_WINDOWS)
    constexpr const char* SYSTEM_LIBRARY  = "kernel32.dll";
    constexpr const char* SYSTEM_FUNCTION = "GetCurrentProcessId";
    using FProcessId = unsigned long (__stdcall*)();
#elif defined(OPAAX_PLATFORM_MACOS)
    constexpr const char* SYSTEM_LIBRARY  = "/usr/lib/libSystem.B.dylib";
    constexpr const char* SYSTEM_FUNCTION = "getpid";
    using FProcessId = int (*)();
#else
    constexpr const char* SYSTEM_LIBRARY  = "libc.so.6";
    constexpr const char* SYSTEM_FUNCTION = "getpid";
    using FProcessId = int (*)();
#endif
}

TEST_CASE("DynamicLibrary: opens the system library and calls a function found in it")
{
    DynamicLibrary lLibrary;
    REQUIRE_MESSAGE(lLibrary.Open(SYSTEM_LIBRARY), lLibrary.GetError().CStr());
    CHECK(lLibrary.IsOpen());
    CHECK(lLibrary.GetPath() == SYSTEM_LIBRARY);
    CHECK(lLibrary.GetError().IsEmpty());

    const FProcessId lProcessId = lLibrary.FindFunction<FProcessId>(SYSTEM_FUNCTION);
    REQUIRE(lProcessId != nullptr);
    CHECK(lProcessId() > 0);
    CHECK(lLibrary.GetError().IsEmpty());

    lLibrary.Close();
    CHECK_FALSE(lLibrary.IsOpen());
}

TEST_CASE("DynamicLibrary: a missing symbol is null and says which")
{
    DynamicLibrary lLibrary;
    REQUIRE(lLibrary.Open(SYSTEM_LIBRARY));

    CHECK(lLibrary.FindSymbol("OpaaxNoSuchFunction") == nullptr);
    CHECK(lLibrary.GetError().Contains("OpaaxNoSuchFunction"));

    // A later success clears the error.
    CHECK(lLibrary.FindSymbol(SYSTEM_FUNCTION) != nullptr);
    CHECK(lLibrary.GetError().IsEmpty());
}

TEST_CASE("DynamicLibrary: a missing file, or a file that is no library, fails with a reason")
{
    const TestTempDir lTemp("dynamic_library");

    DynamicLibrary lLibrary;
    CHECK_FALSE(lLibrary.Open(lTemp.Sub("Missing.dll")));
    CHECK_FALSE(lLibrary.IsOpen());
    CHECK_FALSE(lLibrary.GetError().IsEmpty());

    REQUIRE(FileIO::WriteAllText(lTemp.Sub("NotALibrary.dll"), "plain text"));
    CHECK_FALSE(lLibrary.Open(lTemp.Sub("NotALibrary.dll")));
    CHECK_FALSE(lLibrary.GetError().IsEmpty());

    // Nothing open: no symbol, and a reason.
    CHECK(lLibrary.FindSymbol(SYSTEM_FUNCTION) == nullptr);
    CHECK_FALSE(lLibrary.GetError().IsEmpty());
}

TEST_CASE("DynamicLibrary: a move hands the open library over")
{
    DynamicLibrary lFirst;
    REQUIRE(lFirst.Open(SYSTEM_LIBRARY));

    DynamicLibrary lSecond(Move(lFirst));
    CHECK_FALSE(lFirst.IsOpen());
    CHECK(lSecond.IsOpen());
    CHECK(lSecond.FindSymbol(SYSTEM_FUNCTION) != nullptr);

    DynamicLibrary lThird;
    lThird = Move(lSecond);
    CHECK_FALSE(lSecond.IsOpen());
    CHECK(lThird.FindSymbol(SYSTEM_FUNCTION) != nullptr);
}
