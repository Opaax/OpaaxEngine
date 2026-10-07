// Suite: the engine's own type names (Core/Reflection/TypeInfo.h) against entt's. A component
// registered without a name is saved under its derived leaf name, and that name used to come from
// entt: any difference would change the keys written in map files.
#include <doctest.h>

#include <string>

#include <entt/entt.hpp>

#include "Core/Reflection/TypeInfo.h"
#include "Resources/ResourcePath.h"
#include "Renderer/Components/QuadComponent.h"
#include "World/Components/TransformComponent.h"

using namespace Opaax;

struct GlobalNamespaceProbe {};
class  GlobalClassProbe {};

namespace TypeNameProbes
{
    struct Nested {};
    enum class EKind : Uint8 { A, B };
    struct TextureLike {};
}

namespace
{
    template<typename T>
    void CheckSameAsEntt()
    {
        // As strings, so a failure prints both names.
        const std::string lOurs(TypeNameOf<T>());
        const std::string lEntt(entt::type_name<T>::value());
        CHECK(lOurs == lEntt);
    }
}

TEST_CASE("TypeNameOf: spells every kind of type exactly as entt did")
{
    CheckSameAsEntt<QuadComponent>();
    CheckSameAsEntt<TransformComponent>();
    CheckSameAsEntt<GlobalNamespaceProbe>();
    CheckSameAsEntt<GlobalClassProbe>();
    CheckSameAsEntt<TypeNameProbes::Nested>();
    CheckSameAsEntt<TypeNameProbes::EKind>();
    CheckSameAsEntt<TResourcePath<TypeNameProbes::TextureLike>>();
    CheckSameAsEntt<int>();
}

TEST_CASE("DeriveTypeLeafName: namespace and keyword stripped, whatever the namespace")
{
    CHECK(DeriveTypeLeafName<QuadComponent>() == OpaaxStringID("QuadComponent"));
    CHECK(DeriveTypeLeafName<GlobalNamespaceProbe>() == OpaaxStringID("GlobalNamespaceProbe"));
    CHECK(DeriveTypeLeafName<GlobalClassProbe>() == OpaaxStringID("GlobalClassProbe"));
    CHECK(DeriveTypeLeafName<TypeNameProbes::Nested>() == OpaaxStringID("Nested"));
}

TEST_CASE("TypeIdOf: one id per type, the same every time it is asked")
{
    CHECK(TypeIdOf<QuadComponent>() == TypeIdOf<QuadComponent>());
    CHECK(TypeIdOf<QuadComponent>() != TypeIdOf<TransformComponent>());
    CHECK(TypeIdOf<TypeNameProbes::Nested>() != TypeIdOf<GlobalNamespaceProbe>());

    // Usable at compile time.
    static_assert(TypeIdOf<GlobalNamespaceProbe>() != 0);
}
