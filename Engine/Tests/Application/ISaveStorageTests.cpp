// Suite: ISaveStorage — a player's save slots as files in one folder: names, reading and writing a
// slot, listing, deleting, a write the disk refuses, and the null storage. Each case works in its
// own temp directory.
#include <doctest.h>

#include "Application/Services/ISaveStorage.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/String/OpaaxPathString.h"

#include "../TestTempDir.h"

using namespace Opaax;

// =============================================================================
// Slot names
// =============================================================================

TEST_CASE("ISaveStorage: a slot name is a plain file name on every platform")
{
    const OpaaxString l64(OpaaxString("0123456789012345678901234567890123456789012345678901234567890123"));
    REQUIRE(l64.GetLength() == 64);

    const char* const lValid[] = { "Main", "slot_2", "Ghost-Level-01", "CONSOLE", "NULL", "COM10", "Lpt" };
    for (const char* lName : lValid)
    {
        CAPTURE(lName);
        CHECK(ISaveStorage::IsValidSlotName(lName));
    }
    CHECK(ISaveStorage::IsValidSlotName(l64));

    const char* const lInvalid[] = {
        "", ".", "..", "../Escape", "a/b", "a\\b", "C:", "Main.opaaxsave", "with space", "Zo\xC3\xA9",
        "CON", "nul", "Aux", "prn", "Com1", "LPT9", "com0"
    };
    for (const char* lName : lInvalid)
    {
        CAPTURE(lName);
        CHECK_FALSE(ISaveStorage::IsValidSlotName(lName));
    }
    CHECK_FALSE(ISaveStorage::IsValidSlotName(l64 + "4"));
}

// =============================================================================
// Reading and writing
// =============================================================================

TEST_CASE("SaveStorage: a slot written reads back, from <folder>/<slot>.opaaxsave")
{
    // A user folder with an accent, as %APPDATA% has for many players.
    const TestTempDir lTemp("save_storage_rw");
    const OpaaxString lFolder = lTemp.Sub("Zo\xC3\xA9/SaveGames");
    SaveStorage       lStorage(lFolder);

    CHECK(lStorage.GetSaveDirectory() == lFolder);
    CHECK(lStorage.GetSlotPath("Main") == PathString::Join(lFolder, "Main.opaaxsave"));
    CHECK_FALSE(lStorage.Exists("Main"));

    // The folder is made by the first write.
    REQUIRE(lStorage.Write("Main", "{ \"Coins\": 3 }") == ESaveResult::Ok);
    CHECK(lStorage.Exists("Main"));
    CHECK(FileSystem::IsFile(PathString::Join(lFolder, "Main.opaaxsave")));
    CHECK_FALSE(FileSystem::Exists(FileIO::AtomicTempPath(lStorage.GetSlotPath("Main"))));

    OpaaxString lText;
    REQUIRE(lStorage.Read("Main", lText) == ESaveResult::Ok);
    CHECK(lText == "{ \"Coins\": 3 }");

    // An empty slot file reads back empty, not as a failure.
    REQUIRE(lStorage.Write("Empty", OpaaxString()) == ESaveResult::Ok);
    lText = "before";
    CHECK(lStorage.Read("Empty", lText) == ESaveResult::Ok);
    CHECK(lText.IsEmpty());
}

TEST_CASE("SaveStorage: a slot without a file is Missing, and the text is left as it was")
{
    const TestTempDir lTemp("save_storage_missing");
    SaveStorage       lStorage(lTemp.Sub("SaveGames"));

    OpaaxString lText("kept");
    CHECK(lStorage.Read("Main", lText) == ESaveResult::Missing);
    CHECK(lText == "kept");
    CHECK_FALSE(lStorage.Exists("Main"));
    CHECK(lStorage.Delete("Main") == ESaveResult::Missing);
}

TEST_CASE("SaveStorage: an invalid slot name is refused by every call, and touches no file")
{
    const TestTempDir lTemp("save_storage_invalid");
    SaveStorage       lStorage(lTemp.Sub("SaveGames"));

    OpaaxString lText("kept");
    CHECK(lStorage.Write("../Escape", "x") == ESaveResult::InvalidSlot);
    CHECK(lStorage.Read("../Escape", lText) == ESaveResult::InvalidSlot);
    CHECK(lStorage.Delete("../Escape") == ESaveResult::InvalidSlot);
    CHECK_FALSE(lStorage.Exists("../Escape"));
    CHECK(lStorage.GetSlotPath("../Escape").IsEmpty());

    CHECK(lText == "kept");
    CHECK_FALSE(FileSystem::Exists(lTemp.Sub("Escape.opaaxsave")));
    CHECK_FALSE(FileSystem::Exists(lTemp.Sub("SaveGames")));
}

TEST_CASE("SaveStorage: a write the disk refuses is IoError, and the slot keeps its last save")
{
    const TestTempDir lTemp("save_storage_refused");
    SaveStorage       lStorage(lTemp.Sub("SaveGames"));

    REQUIRE(lStorage.Write("Main", "good") == ESaveResult::Ok);

    // A folder where the write's temporary file goes: the write cannot happen.
    REQUIRE(FileSystem::CreateDirectories(FileIO::AtomicTempPath(lStorage.GetSlotPath("Main"))));

    CHECK(lStorage.Write("Main", "next") == ESaveResult::IoError);

    OpaaxString lText;
    REQUIRE(lStorage.Read("Main", lText) == ESaveResult::Ok);
    CHECK(lText == "good");
}

// =============================================================================
// Listing and deleting
// =============================================================================

TEST_CASE("SaveStorage: ListSlots lists the slot files, sorted, and nothing else")
{
    const TestTempDir lTemp("save_storage_list");
    const OpaaxString lFolder = lTemp.Sub("SaveGames");
    SaveStorage       lStorage(lFolder);

    // No folder yet: no slots.
    CHECK(lStorage.ListSlots().empty());

    REQUIRE(lStorage.Write("Settings", "s") == ESaveResult::Ok);
    REQUIRE(lStorage.Write("Main", "m") == ESaveResult::Ok);
    REQUIRE(lStorage.Write("Ghost_01", "g") == ESaveResult::Ok);

    // What else a folder may hold: a crashed write, notes, a badly named file, a folder.
    REQUIRE(FileIO::WriteAllText(PathString::Join(lFolder, "Main.opaaxsave.tmp"), "half"));
    REQUIRE(FileIO::WriteAllText(PathString::Join(lFolder, "Notes.txt"), "n"));
    REQUIRE(FileIO::WriteAllText(PathString::Join(lFolder, "bad name.opaaxsave"), "b"));
    REQUIRE(FileSystem::CreateDirectories(PathString::Join(lFolder, "Folder.opaaxsave")));

    const TDynArray<OpaaxString> lSlots = lStorage.ListSlots();
    REQUIRE(lSlots.size() == 3);
    CHECK(lSlots[0] == "Ghost_01");
    CHECK(lSlots[1] == "Main");
    CHECK(lSlots[2] == "Settings");
}

TEST_CASE("SaveStorage: Delete removes the slot and what a crashed write left beside it")
{
    const TestTempDir lTemp("save_storage_delete");
    SaveStorage       lStorage(lTemp.Sub("SaveGames"));

    REQUIRE(lStorage.Write("Main", "m") == ESaveResult::Ok);
    const OpaaxString lTempPath = FileIO::AtomicTempPath(lStorage.GetSlotPath("Main"));
    REQUIRE(FileIO::WriteAllText(lTempPath, "half"));

    CHECK(lStorage.Delete("Main") == ESaveResult::Ok);
    CHECK_FALSE(lStorage.Exists("Main"));
    CHECK_FALSE(FileSystem::Exists(lTempPath));
    CHECK(lStorage.ListSlots().empty());

    CHECK(lStorage.Delete("Main") == ESaveResult::Missing);
}

// =============================================================================
// Null storage
// =============================================================================

TEST_CASE("ISaveStorage::Null: nothing is kept, every slot is missing")
{
    ISaveStorage& lNull = ISaveStorage::Null();

    OpaaxString lText("kept");
    CHECK(lNull.IsNull());
    CHECK(lNull.GetSaveDirectory().IsEmpty());
    CHECK(lNull.GetSlotPath("Main").IsEmpty());
    CHECK(lNull.Write("Main", "x") == ESaveResult::IoError);
    CHECK(lNull.Read("Main", lText) == ESaveResult::Missing);
    CHECK(lText == "kept");
    CHECK_FALSE(lNull.Exists("Main"));
    CHECK(lNull.Delete("Main") == ESaveResult::Missing);
    CHECK(lNull.ListSlots().empty());
}
