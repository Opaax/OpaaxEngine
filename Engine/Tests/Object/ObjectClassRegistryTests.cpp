// Suite: ObjectClassRegistry — the classes a file can name, their aliases, and what an editor may make.
#include <doctest.h>

#include "Engine/Registries/AutoRegistration.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Object/ObjectClassRegistry.h"
#include "Object/ObjectMacros.h"

using namespace Opaax;

namespace ClassRegistryTestTypes
{
    class RegistryBase : public Object
    {
        OPAAX_ABSTRACT_OBJECT(RegistryBase, Object)
    };

    class RegistryLamp final : public RegistryBase
    {
        OPAAX_OBJECT(RegistryLamp, RegistryBase)

    public:
        float Brightness = 1.f;
        OPAAX_PROPERTIES(RegistryLamp, OPAAX_PROP(Brightness))
    };

    class RegistryDoor final : public RegistryBase
    {
        OPAAX_OBJECT(RegistryDoor, RegistryBase)
    };

    // Registered by the macro, the way a game module does it.
    class RegistryAutoChest final : public Object
    {
        OPAAX_OBJECT(RegistryAutoChest, Object)
    };

    OPAAX_REGISTER_OBJECT(RegistryAutoChest);
    OPAAX_REGISTER_OBJECT_ALIAS("RegistryOldChest", "RegistryAutoChest");
}

// Same class name as above, another namespace: the saved name would be ambiguous.
namespace ClassRegistryOtherTypes
{
    class RegistryLamp final : public Object
    {
        OPAAX_OBJECT(RegistryLamp, Object)
    };
}

using namespace ClassRegistryTestTypes;

TEST_CASE("ObjectClassRegistry: a class registers under its C++ name, once")
{
    ObjectClassRegistry lRegistry;

    REQUIRE(lRegistry.Register<RegistryLamp>());
    CHECK_FALSE(lRegistry.Register<RegistryLamp>());                            // same class twice
    CHECK_FALSE(lRegistry.Register<ClassRegistryOtherTypes::RegistryLamp>());   // same name, other type

    CHECK(lRegistry.FindByName(OPAAX_ID("RegistryLamp")) == &RegistryLamp::StaticClass());
    CHECK(lRegistry.FindByTypeId(TypeIdOf<RegistryLamp>()) == &RegistryLamp::StaticClass());
    CHECK(lRegistry.FindByName(OPAAX_ID("RegistryDoor")) == nullptr);
    CHECK(lRegistry.Count() == 1u);

    lRegistry.Seal();
    CHECK(lRegistry.IsSealed());
    CHECK_FALSE(lRegistry.Register<RegistryDoor>());
    CHECK(lRegistry.Count() == 1u);
}

TEST_CASE("ObjectClassRegistry: an alias loads an old name as the current class, and keeps that name taken")
{
    ObjectClassRegistry lRegistry;
    REQUIRE(lRegistry.Register<RegistryLamp>());

    CHECK(lRegistry.AddAlias(OPAAX_ID("OldLamp"), OPAAX_ID("RegistryLamp")));
    CHECK(lRegistry.FindByName(OPAAX_ID("OldLamp")) == &RegistryLamp::StaticClass());

    CHECK_FALSE(lRegistry.AddAlias(OPAAX_ID("Ghost"), OPAAX_ID("NoSuchClass")));   // to nothing
    CHECK_FALSE(lRegistry.AddAlias(OPAAX_ID("RegistryLamp"), OPAAX_ID("RegistryLamp")));   // over a class
    CHECK_FALSE(lRegistry.AddAlias(OPAAX_ID("OldLamp"), OPAAX_ID("RegistryLamp")));        // twice

    // The old name stays taken, or one file name would mean two classes.
    CHECK(lRegistry.Count() == 1u);
    lRegistry.Seal();
    CHECK_FALSE(lRegistry.AddAlias(OPAAX_ID("Later"), OPAAX_ID("RegistryLamp")));
}

TEST_CASE("ObjectClassRegistry: the classes of a base, the abstract ones left out on demand")
{
    ObjectClassRegistry lRegistry;
    REQUIRE(lRegistry.Register<RegistryBase>());
    REQUIRE(lRegistry.Register<RegistryLamp>());
    REQUIRE(lRegistry.Register<RegistryDoor>());
    REQUIRE(lRegistry.Register<RegistryAutoChest>());

    const TDynArray<const ObjectClass*> lAll = lRegistry.GetClassesOf(RegistryBase::StaticClass(), false);
    REQUIRE(lAll.size() == 3);
    CHECK(lAll[0] == &RegistryBase::StaticClass());   // registration order
    CHECK(lAll[1] == &RegistryLamp::StaticClass());
    CHECK(lAll[2] == &RegistryDoor::StaticClass());

    const TDynArray<const ObjectClass*> lConcrete = lRegistry.GetClassesOf(RegistryBase::StaticClass(), true);
    REQUIRE(lConcrete.size() == 2);
    CHECK(lConcrete[0] == &RegistryLamp::StaticClass());

    CHECK(lRegistry.GetClassesOf(Object::StaticClass(), true).size() == 3);
}

TEST_CASE("OPAAX_REGISTER_OBJECT: a class and its alias registered by one line each")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);
    RunAutoRegistrations(lRegistrar);

    const ObjectClassRegistry& lClasses = lRegistries.ObjectClasses();
    CHECK(lClasses.FindByName(OPAAX_ID("RegistryAutoChest")) == &RegistryAutoChest::StaticClass());
    CHECK(lClasses.FindByName(OPAAX_ID("RegistryOldChest")) == &RegistryAutoChest::StaticClass());

    lRegistries.SealAll();
    CHECK(lClasses.IsSealed());
}
