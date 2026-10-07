// Suite: GLSLPort — shaders written for GLSL 4.50 with explicit bindings run as GLSL 4.10 core
// (macOS). The version line is rewritten and each `binding = N` taken out of the source and
// returned, for the backend to apply after linking. Pure text: no GL context needed.
#include <doctest.h>

#include <string>

#include "RHI/OpenGL/GLSLPort.h"

using namespace Opaax;

namespace
{
    std::string Port(const std::string& InSource, TDynArray<GLSLPort::ResourceBinding>& OutBindings)
    {
        return GLSLPort::To410(InSource, OutBindings);
    }
}

TEST_CASE("GLSLPort: the version line becomes 4.10 core")
{
    TDynArray<GLSLPort::ResourceBinding> lBindings;
    const std::string lOut = Port("#version 450 core\nvoid main() {}\n", lBindings);

    CHECK(lOut == "#version 410 core\nvoid main() {}\n");
    CHECK(lBindings.empty());
}

TEST_CASE("GLSLPort: a uniform block's binding is taken out, its other qualifiers kept")
{
    TDynArray<GLSLPort::ResourceBinding> lBindings;
    const std::string lOut = Port("layout(std140, binding = 1) uniform CameraUBO\n{\n    mat4 u_ViewProjection;\n};\n", lBindings);

    CHECK(lOut == "layout(std140) uniform CameraUBO\n{\n    mat4 u_ViewProjection;\n};\n");
    REQUIRE(lBindings.size() == 1);
    CHECK(lBindings[0].Name == "CameraUBO");
    CHECK(lBindings[0].Binding == 1);
    CHECK(lBindings[0].bBlock);
}

TEST_CASE("GLSLPort: a sampler's binding is taken out with its whole layout")
{
    TDynArray<GLSLPort::ResourceBinding> lBindings;

    SUBCASE("an array: its units follow the binding")
    {
        const std::string lOut = Port("layout(binding = 0) uniform sampler2D u_Textures[16];\n", lBindings);

        CHECK(lOut == " uniform sampler2D u_Textures[16];\n");
        REQUIRE(lBindings.size() == 1);
        CHECK(lBindings[0].Name == "u_Textures");
        CHECK(lBindings[0].Binding == 0);
        CHECK(lBindings[0].ArraySize == 16);
        CHECK_FALSE(lBindings[0].bBlock);
    }

    SUBCASE("a single sampler, odd spacing")
    {
        const std::string lOut = Port("layout( binding=3 )   uniform   sampler2D   u_Shadow ;\n", lBindings);

        CHECK(lOut.find("binding") == std::string::npos);
        CHECK(lOut.find("uniform   sampler2D   u_Shadow ;") != std::string::npos);
        REQUIRE(lBindings.size() == 1);
        CHECK(lBindings[0].Name == "u_Shadow");
        CHECK(lBindings[0].Binding == 3);
        CHECK(lBindings[0].ArraySize == 1);
    }
}

TEST_CASE("GLSLPort: everything else is left as written")
{
    TDynArray<GLSLPort::ResourceBinding> lBindings;

    const std::string lLocations = "layout(location = 0) in vec3 a_Position;\nlayout(location = 0) out vec4 FragColor;\n";
    CHECK(Port(lLocations, lBindings) == lLocations);

    // A word that only contains "layout", a qualifier that only starts like "binding".
    const std::string lLookalikes = "float mylayout(float x) { return x; }\nlayout(bindings = 2) uniform Block {};\n";
    CHECK(Port(lLookalikes, lBindings) == lLookalikes);
    CHECK(lBindings.empty());
}

TEST_CASE("GLSLPort: a full shader keeps its bindings in source order")
{
    const std::string lSource =
        "#version 450 core\n"
        "layout(location = 0) in vec2 v_UV;\n"
        "layout(std140, binding = 1) uniform CameraUBO { mat4 u_ViewProjection; };\n"
        "layout(binding = 0) uniform sampler2D u_Textures[16];\n"
        "layout(binding = 16) uniform sampler2D u_Lights;\n"
        "layout(location = 0) out vec4 FragColor;\n"
        "void main() { FragColor = texture(u_Lights, v_UV); }\n";

    TDynArray<GLSLPort::ResourceBinding> lBindings;
    const std::string lOut = Port(lSource, lBindings);

    CHECK(lOut.rfind("#version 410 core\n", 0) == 0);
    CHECK(lOut.find("binding") == std::string::npos);
    CHECK(lOut.find("layout(location = 0) in vec2 v_UV;") != std::string::npos);

    REQUIRE(lBindings.size() == 3);
    CHECK(lBindings[0].Name == "CameraUBO");
    CHECK(lBindings[1].Name == "u_Textures");
    CHECK(lBindings[2].Name == "u_Lights");
    CHECK(lBindings[2].Binding == 16);
}
