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
