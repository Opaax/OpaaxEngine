// Suite: LinearColor — a distinct TYPE over Vector4F, so the editor can dispatch a picker on it.
//
// The load-bearing claim is the one about BYTES: a colour must serialize exactly as the vector it
// replaced, or every .opaaxmap already on disk holds a key the component can no longer read — and
// with NLOHMANN_..._WITH_DEFAULT that failure is SILENT (the field reverts to white) rather than
// loud. So the round trip is not enough; the two dumps are compared directly.
#include <doctest.h>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "World/Components/DummyComponent.h"

using namespace Opaax;

TEST_CASE("LinearColor: serializes byte-for-byte as the Vector4F it replaced")
{
    const LinearColor lColour(0.25f, 0.5f, 0.75f, 1.f);
    const Vector4F    lVector(0.25f, 0.5f, 0.75f, 1.f);

    CHECK(nlohmann::json(lColour).dump() == nlohmann::json(lVector).dump());
}

TEST_CASE("LinearColor: a component's colour keeps the key shape already on disk")
{
    const nlohmann::json lPayload = nlohmann::json(DummyComponent{})["Color"];

    // What every saved map holds under that key: the vector's four named components, not r/g/b/a.
    REQUIRE(lPayload.is_object());
    CHECK(lPayload.contains("x"));
    CHECK(lPayload.contains("y"));
    CHECK(lPayload.contains("z"));
    CHECK(lPayload.contains("w"));
}

TEST_CASE("LinearColor: a payload written as a plain vector still loads")
{
    // Literally the bytes a pre-LinearColor build wrote.
    const nlohmann::json lOld = {{"x", 1.f}, {"y", 0.f}, {"z", 0.f}, {"w", 1.f}};

    LinearColor lLoaded;
    lOld.get_to(lLoaded);

    CHECK(lLoaded.r == doctest::Approx(1.f));
    CHECK(lLoaded.g == doctest::Approx(0.f));
    CHECK(lLoaded.a == doctest::Approx(1.f));
}

TEST_CASE("LinearColor: converts to a vector implicitly, both ways")
{
    // What keeps every renderer call site compiling — DrawQuad and RenderSystemDesc speak vectors.
    const LinearColor lColour(0.1f, 0.2f, 0.3f, 0.4f);
    const Vector4F    lAsVector = lColour;

    CHECK(lAsVector.b == doctest::Approx(0.3f));

    const LinearColor lBack = lAsVector;
    CHECK(lBack == lColour);

    // And it reads as a colour OR as a vector, which is why it derives rather than wraps.
    CHECK(lColour.r == doctest::Approx(lColour.x));
    CHECK(lColour.a == doctest::Approx(lColour.w));
}
