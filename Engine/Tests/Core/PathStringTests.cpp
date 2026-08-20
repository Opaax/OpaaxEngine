// Suite: PathString::Stem — the ONE path-stem rule, previously copied into MapFile::StemId and
// LevelFile's FileStem. MapFileTests pins the same shapes through StemId; these pin them directly,
// plus the edges neither caller ever expressed.
#include <doctest.h>

#include "Core/String/OpaaxPathString.h"
#include "Core/String/OpaaxString.hpp"

using namespace Opaax;

static_assert(PathString::Stem("Maps/Decor.opaaxmap") == "Decor");

TEST_CASE("PathString::Stem: the filename without its extension")
{
    CHECK(PathString::Stem("C:/Proj/Assets/Maps/Decor.opaaxmap") == "Decor");
    CHECK(PathString::Stem("C:\\Proj\\Assets\\Maps\\Decor.opaaxmap") == "Decor");
    CHECK(PathString::Stem("C:/Proj\\Mixed/Sep\\Decor.opaaxmap") == "Decor");
    CHECK(PathString::Stem("Decor.opaaxmap") == "Decor");
    CHECK(PathString::Stem("Decor") == "Decor");
}

TEST_CASE("PathString::Stem: only the LAST dot of the filename ends the stem")
{
    CHECK(PathString::Stem("Maps/No.Dots.Here.opaaxmap") == "No.Dots.Here");

    // A dot in a DIRECTORY is not an extension — the rule that needs the two scans in this order.
    CHECK(PathString::Stem("C:/a.b/Maps/Decor.opaaxmap") == "Decor");
    CHECK(PathString::Stem("C:/a.b/Maps/Decor") == "Decor");
}

TEST_CASE("PathString::Stem: a path with no stem answers empty")
{
    CHECK(PathString::Stem("").IsEmpty());
    CHECK(PathString::Stem("Maps/").IsEmpty());
    CHECK(PathString::Stem("C:\\Maps\\").IsEmpty());
    CHECK(PathString::Stem(".gitignore").IsEmpty());        // a dotfile is all extension
    CHECK(PathString::Stem("Maps/.gitignore").IsEmpty());
    CHECK(PathString::Stem(".").IsEmpty());
}

TEST_CASE("PathString::Stem: the result VIEWS the path it was given, it does not copy it")
{
    const OpaaxString     lPath("C:/Proj/Maps/Decor.opaaxmap");
    const OpaaxStringView lStem = PathString::Stem(lPath);

    CHECK(lStem == "Decor");
    CHECK(lStem.Data() == lPath.CStr() + 13);   // points INTO the argument's own bytes
    CHECK(lStem.ToString() == "Decor");         // ...and ToString is how you keep it
}

// -----------------------------------------------------------------------------
// PathString::Extension — Stem's other half, and the reason both live here: the editor's scanner
// had its own copy of this rule while the engine now owns the extension -> resource type table.
// -----------------------------------------------------------------------------
static_assert(PathString::Extension("Maps/Decor.opaaxmap") == ".opaaxmap");

TEST_CASE("PathString::Extension: everything after the last dot, dot included")
{
    CHECK(PathString::Extension("C:/Proj/Assets/Maps/Decor.opaaxmap") == ".opaaxmap");
    CHECK(PathString::Extension("C:\Proj\Maps\Decor.opaaxmap") == ".opaaxmap");
    CHECK(PathString::Extension("Decor.opaaxmap") == ".opaaxmap");

    // The LAST dot wins, so a dotted stem keeps only its real extension.
    CHECK(PathString::Extension("Maps/No.Dots.Here.opaaxmap") == ".opaaxmap");

    // Case is UNTOUCHED here — comparability is NormalizeExtension's job, not this one's.
    CHECK(PathString::Extension("Maps/Decor.OPAAXMAP") == ".OPAAXMAP");
}

TEST_CASE("PathString::Extension: a path with no extension answers empty")
{
    CHECK(PathString::Extension("").IsEmpty());
    CHECK(PathString::Extension("Decor").IsEmpty());
    CHECK(PathString::Extension("Maps/").IsEmpty());

    // A dot in a DIRECTORY is not an extension — the same two-scan order Stem needs.
    CHECK(PathString::Extension("C:/a.b/Maps/Decor").IsEmpty());

    // A dotfile is all extension and no stem, so it reports NEITHER (Stem agrees, above).
    CHECK(PathString::Extension(".gitignore").IsEmpty());
    CHECK(PathString::Extension("Maps/.gitignore").IsEmpty());
    CHECK(PathString::Extension(".").IsEmpty());
}

TEST_CASE("PathString::Extension: a trailing dot IS an extension, an empty one")
{
    // Not a curiosity: it is what stops "Decor." from being read as "Decor" + no extension, which
    // would make a file with a stray dot silently resolve to the wrong resource type.
    CHECK(PathString::Extension("Decor.") == ".");
}

TEST_CASE("PathString::Extension: the result VIEWS the path it was given")
{
    const OpaaxString     lPath("C:/Proj/Maps/Decor.opaaxmap");
    const OpaaxStringView lExt = PathString::Extension(lPath);

    CHECK(lExt == ".opaaxmap");
    CHECK(lExt.Data() == lPath.CStr() + 18);   // points INTO the argument's own bytes
}
