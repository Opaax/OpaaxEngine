// Suite: OpenGLDebug — how a driver message is sorted and named, and that a repeated one is
// logged once. Plain numbers: no GL context needed.
#include <doctest.h>

#include "Core/String/OpaaxStringView.hpp"
#include "RHI/OpenGL/OpenGLDebug.h"

using namespace Opaax;

namespace
{
    // The GL values, as the specification gives them.
    constexpr Uint32 TYPE_ERROR        = 0x824C;
    constexpr Uint32 TYPE_DEPRECATED   = 0x824D;
    constexpr Uint32 TYPE_UNDEFINED    = 0x824E;
    constexpr Uint32 TYPE_PORTABILITY  = 0x824F;
    constexpr Uint32 TYPE_PERFORMANCE  = 0x8250;
    constexpr Uint32 TYPE_OTHER        = 0x8251;
    constexpr Uint32 TYPE_MARKER       = 0x8268;

    constexpr Uint32 SEVERITY_HIGH     = 0x9146;
    constexpr Uint32 SEVERITY_MEDIUM   = 0x9147;
    constexpr Uint32 SEVERITY_LOW      = 0x9148;
    constexpr Uint32 SEVERITY_NOTE     = 0x826B;
}

TEST_CASE("OpenGLDebug: a wrong call is an error, whatever its severity")
{
    CHECK(OpenGLDebug::Classify(TYPE_ERROR, SEVERITY_HIGH)   == OpenGLDebug::EReport::Error);
    CHECK(OpenGLDebug::Classify(TYPE_ERROR, SEVERITY_MEDIUM) == OpenGLDebug::EReport::Error);
    CHECK(OpenGLDebug::Classify(TYPE_ERROR, SEVERITY_LOW)    == OpenGLDebug::EReport::Error);
    CHECK(OpenGLDebug::Classify(TYPE_ERROR, SEVERITY_NOTE)   == OpenGLDebug::EReport::Error);
}

TEST_CASE("OpenGLDebug: doubtful use is a warning when the driver rates it medium or high")
{
    for (const Uint32 lType : { TYPE_DEPRECATED, TYPE_UNDEFINED, TYPE_PORTABILITY, TYPE_PERFORMANCE })
    {
        CHECK(OpenGLDebug::Classify(lType, SEVERITY_HIGH)   == OpenGLDebug::EReport::Warn);
        CHECK(OpenGLDebug::Classify(lType, SEVERITY_MEDIUM) == OpenGLDebug::EReport::Warn);
        CHECK(OpenGLDebug::Classify(lType, SEVERITY_LOW)    == OpenGLDebug::EReport::Ignore);
        CHECK(OpenGLDebug::Classify(lType, SEVERITY_NOTE)   == OpenGLDebug::EReport::Ignore);
    }
}

TEST_CASE("OpenGLDebug: notes and markers are not logged")
{
    CHECK(OpenGLDebug::Classify(TYPE_OTHER,  SEVERITY_HIGH) == OpenGLDebug::EReport::Ignore);
    CHECK(OpenGLDebug::Classify(TYPE_OTHER,  SEVERITY_NOTE) == OpenGLDebug::EReport::Ignore);
    CHECK(OpenGLDebug::Classify(TYPE_MARKER, SEVERITY_NOTE) == OpenGLDebug::EReport::Ignore);
}

TEST_CASE("OpenGLDebug: errors, sources and types have a name")
{
    CHECK(OpaaxStringView(OpenGLDebug::ErrorName(0x0500)) == "GL_INVALID_ENUM");
    CHECK(OpaaxStringView(OpenGLDebug::ErrorName(0x0502)) == "GL_INVALID_OPERATION");
    CHECK(OpaaxStringView(OpenGLDebug::ErrorName(0x0506)) == "GL_INVALID_FRAMEBUFFER_OPERATION");
    CHECK(OpaaxStringView(OpenGLDebug::ErrorName(0x1234)) == "unknown error");

    CHECK(OpaaxStringView(OpenGLDebug::SourceName(0x8246)) == "API");
    CHECK(OpaaxStringView(OpenGLDebug::SourceName(0x8248)) == "shader compiler");
    CHECK(OpaaxStringView(OpenGLDebug::SourceName(0)) == "other");

    CHECK(OpaaxStringView(OpenGLDebug::TypeName(TYPE_ERROR)) == "error");
    CHECK(OpaaxStringView(OpenGLDebug::TypeName(TYPE_PERFORMANCE)) == "performance");
    CHECK(OpaaxStringView(OpenGLDebug::TypeName(TYPE_MARKER)) == "other");
}

TEST_CASE("OpenGLDebug: a message is first once")
{
    OpenGLDebug::SeenMessages lSeen;

    CHECK(lSeen.AddIsFirst(7));
    CHECK_FALSE(lSeen.AddIsFirst(7));
    CHECK(lSeen.AddIsFirst(8));
    CHECK_FALSE(lSeen.AddIsFirst(7));
    CHECK_FALSE(lSeen.IsFull());
}

TEST_CASE("OpenGLDebug: past its capacity, a new message is not first any more")
{
    OpenGLDebug::SeenMessages lSeen;

    for (Uint32 i = 0; i < OpenGLDebug::SeenMessages::CAPACITY; ++i)
    {
        CHECK(lSeen.AddIsFirst(1000 + i));
    }

    CHECK(lSeen.IsFull());
    CHECK_FALSE(lSeen.AddIsFirst(5));
    CHECK_FALSE(lSeen.AddIsFirst(1000));
}
