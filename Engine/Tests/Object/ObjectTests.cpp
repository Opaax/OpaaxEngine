// Suite: Object — the root type: classes and their chain, the object table and its handles, owners,
// references, and properties read and written without the C++ type.
#include <doctest.h>

#include "Core/OpaaxTypes.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectTable.h"

using namespace Opaax;

namespace ObjectTestTypes
{
    // A base never made by itself.
    class TestShape : public Object
    {
        OPAAX_ABSTRACT_OBJECT(TestShape, Object)
    };

    class TestCircle : public TestShape
    {
        OPAAX_OBJECT(TestCircle, TestShape)

    public:
        float Radius = 1.f;

        OPAAX_PROPERTIES(TestCircle, OPAAX_PROP(Radius).SetRange(0.f, 100.f))

        ~TestCircle() override { ++s_Destroyed; }

        static inline Uint32 s_Destroyed = 0;
    };

    class TestRing final : public TestCircle
    {
        OPAAX_OBJECT(TestRing, TestCircle)

    public:
        float Thickness = 0.25f;
        Int32 Segments  = 32;

        OPAAX_PROPERTIES(TestRing, OPAAX_PROP(Thickness), OPAAX_PROP(Segments))
    };

    // No properties of its own: its parent's are its whole list.
    class TestPlainCircle final : public TestCircle
    {
        OPAAX_OBJECT(TestPlainCircle, TestCircle)
    };

    class TestOther final : public Object
    {
        OPAAX_OBJECT(TestOther, Object)
    };

    struct TestLinkGroup
    {
        TObjectPtr<TestCircle> Nested;
        float                  Scale = 1.f;

        OPAAX_PROPERTIES(TestLinkGroup, OPAAX_PROP(Nested), OPAAX_PROP(Scale))
    };

    class TestHolder final : public Object
    {
        OPAAX_OBJECT(TestHolder, Object)

    public:
        TObjectPtr<TestCircle>            Target;
        TDynArray<TObjectPtr<TestCircle>> Many;
        TestLinkGroup                     Group;
        float                             Weight = 2.f;

        OPAAX_PROPERTIES(TestHolder, OPAAX_PROP(Target), OPAAX_PROP(Many), OPAAX_PROP(Group), OPAAX_PROP(Weight))
    };
}

using namespace ObjectTestTypes;

namespace
{
    /** Records "kind name" per visit, so a walk can be compared at once. */
    class NameVisitor final : public IPropertyVisitor
    {
    public:
        TDynArray<OpaaxString> Lines;

        void Visit(const char* InName, bool&, const PropertyMeta&) override          { Add("bool", InName); }
        void Visit(const char* InName, Int16&, const PropertyMeta&) override         { Add("int16", InName); }
        void Visit(const char* InName, Int32&, const PropertyMeta&) override         { Add("int32", InName); }
        void Visit(const char* InName, Uint32&, const PropertyMeta&) override        { Add("uint32", InName); }
        void Visit(const char* InName, float& InValue, const PropertyMeta&) override
        {
            Add("float", InName);
            InValue = 9.f;   // a write through the visitor
        }
        void Visit(const char* InName, Vector2F&, const PropertyMeta&) override      { Add("vec2", InName); }
        void Visit(const char* InName, Vector3F&, const PropertyMeta&) override      { Add("vec3", InName); }
        void Visit(const char* InName, Vector4F&, const PropertyMeta&) override      { Add("vec4", InName); }
        void Visit(const char* InName, LinearColor&, const PropertyMeta&) override   { Add("color", InName); }
        void Visit(const char* InName, OpaaxString&, const PropertyMeta&) override   { Add("string", InName); }
        void Visit(const char* InName, OpaaxStringID&, const PropertyMeta&) override { Add("id", InName); }
        void VisitEnum(const char* InName, const char* const*, Uint32, Uint32&, const PropertyMeta&) override { Add("enum", InName); }
        void VisitResourcePath(const char* InName, OpaaxString&, Uint32, const PropertyMeta&) override { Add("resource", InName); }

        void VisitAssetRef(const char* InName, OpaaxString&, const ObjectClass&, const PropertyMeta&) override { Add("asset", InName); }

        void VisitObjectRef(const char* InName, ObjectHandle&, const ObjectClass& InClass, const PropertyMeta&) override
        {
            Lines.push_back(OpaaxString::Format("object {} {}", InName, InClass.GetName().CStr()));
        }

        bool BeginGroup(const char* InName, const PropertyMeta&) override { Add("group", InName); return true; }
        void EndGroup() override { Lines.emplace_back("end"); }
        void VisitUnsupported(const char* InName, OpaaxStringView) override { Add("unsupported", InName); }

        void VisitList(const char* InName, IPropertyList& InList, const PropertyMeta& InMeta) override
        {
            Add("list", InName);
            IPropertyVisitor::VisitList(InName, InList, InMeta);
        }

    private:
        void Add(const char* InKind, const char* InName) { Lines.push_back(OpaaxString::Format("{} {}", InKind, InName)); }
    };
}

// =============================================================================
// Classes
// =============================================================================

TEST_CASE("ObjectClass: a class knows its name, its parent and whether it can be made")
{
    const ObjectClass& lRing = TestRing::StaticClass();

    CHECK(lRing.GetName() == OPAAX_ID("TestRing"));
    CHECK(lRing.GetTypeId() == TypeIdOf<TestRing>());
    CHECK(lRing.GetSize() == sizeof(TestRing));
    CHECK(lRing.GetSuper() == &TestCircle::StaticClass());
    CHECK(TestCircle::StaticClass().GetSuper() == &TestShape::StaticClass());
    CHECK(TestShape::StaticClass().GetSuper() == &Object::StaticClass());
    CHECK(Object::StaticClass().GetSuper() == nullptr);

    // The same class object every time it is asked.
    CHECK(&TestRing::StaticClass() == &lRing);

    CHECK_FALSE(lRing.IsAbstract());
    CHECK(TestShape::StaticClass().IsAbstract());
    CHECK(Object::StaticClass().IsAbstract());

    CHECK(lRing.IsChildOf(lRing));
    CHECK(lRing.IsChildOf(TestShape::StaticClass()));
    CHECK(lRing.IsChildOf(Object::StaticClass()));
    CHECK_FALSE(TestCircle::StaticClass().IsChildOf(lRing));
    CHECK_FALSE(lRing.IsChildOf(TestOther::StaticClass()));
}

TEST_CASE("Object: GetClass is the dynamic type's; IsA and Cast follow the chain")
{
    TestRing lRing;
    Object&  lAsObject = lRing;

    CHECK(&lAsObject.GetClass() == &TestRing::StaticClass());
    CHECK(lAsObject.IsA<TestRing>());
    CHECK(lAsObject.IsA<TestCircle>());
    CHECK(lAsObject.IsA<Object>());
    CHECK_FALSE(lAsObject.IsA<TestOther>());

    CHECK(Cast<TestCircle>(&lAsObject) == &lRing);
    CHECK(Cast<TestOther>(&lAsObject) == nullptr);
    CHECK(Cast<TestCircle>(static_cast<Object*>(nullptr)) == nullptr);

    const Object& lConst = lRing;
    CHECK(Cast<TestRing>(&lConst) == &lRing);
}

// =============================================================================
// The table
// =============================================================================

TEST_CASE("ObjectTable: every object has a slot; a handle reads null once the object is gone")
{
    ObjectTable& lTable  = ObjectTable::Get();
    const Uint32 lBefore = lTable.Count();

    ObjectHandle lHandle;
    {
        TestOther lObject;
        lHandle = lObject.GetHandle();

        CHECK(lHandle.IsValid());
        CHECK(lTable.Count() == lBefore + 1);
        CHECK(lTable.Resolve(lHandle) == &lObject);
        CHECK(lObject.GetFlags() == EObjectFlags::None);   // not made by NewObject
    }

    CHECK(lTable.Resolve(lHandle) == nullptr);
    CHECK(lTable.Count() == lBefore);

    // The freed slot is reused with a new generation: the old handle never names the new object.
    TestOther lNext;
    CHECK(lNext.GetHandle().Index == lHandle.Index);
    CHECK(lNext.GetHandle().Generation != lHandle.Generation);
    CHECK(lTable.Resolve(lHandle) == nullptr);
    CHECK(lTable.Resolve(lNext.GetHandle()) == &lNext);

    CHECK(lTable.Resolve(ObjectHandle{}) == nullptr);
    CHECK(lTable.Resolve(ObjectHandle{ 0xFFFFFFu, 1 }) == nullptr);
}

TEST_CASE("ObjectTable: a snapshot lists the live objects only")
{
    TestOther lA;
    ObjectHandle lGone;
    {
        TestOther lB;
        lGone = lB.GetHandle();
    }

    const TDynArray<ObjectHandle> lSnapshot = ObjectTable::Get().Snapshot();

    bool bHasA    = false;
    bool bHasGone = false;
    for (const ObjectHandle& lHandle : lSnapshot)
    {
        bHasA    = bHasA || lHandle == lA.GetHandle();
        bHasGone = bHasGone || lHandle == lGone;
    }

    CHECK(bHasA);
    CHECK_FALSE(bHasGone);
    CHECK(lSnapshot.size() == ObjectTable::Get().Count());
}

TEST_CASE("ObjectTable: objects made and destroyed on several threads get their own slots")
{
    const Uint32 lBefore = ObjectTable::Get().Count();

    TAtomic<Uint32>   lMismatches{ 0 };
    TDynArray<Thread> lThreads;

    for (Int32 lThread = 0; lThread < 4; ++lThread)
    {
        lThreads.emplace_back([&lMismatches]
        {
            for (Int32 lIndex = 0; lIndex < 200; ++lIndex)
            {
                TStrongObjectPtr<TestOther> lObject = NewObject<TestOther>();

                if (ObjectTable::Get().Resolve(lObject->GetHandle()) != lObject.Get())
                {
                    ++lMismatches;
                }
            }
        });
    }

    for (Thread& lThread : lThreads)
    {
        lThread.join();
    }

    CHECK(lMismatches.load() == 0);
    CHECK(ObjectTable::Get().Count() == lBefore);
}

// =============================================================================
// Owners and handles
// =============================================================================

TEST_CASE("NewObject: owned by strong pointers; the last one destroys the object")
{
    const Uint32 lDestroyedBefore = TestCircle::s_Destroyed;

    TStrongObjectPtr<TestCircle> lFirst = NewObject<TestCircle>();
    REQUIRE(lFirst.IsValid());

    const ObjectHandle lHandle = lFirst->GetHandle();
    CHECK(lFirst->HasAnyFlags(EObjectFlags::Managed));
    CHECK(ObjectTable::Get().GetStrongRefCount(lHandle) == 1);

    // Named after its class and slot unless a name is given.
    CHECK(lFirst->GetName() == OpaaxStringID(OpaaxString::Format("TestCircle_{}", lHandle.Index)));
    CHECK(NewObject<TestCircle>(OPAAX_ID("Named"))->GetName() == OPAAX_ID("Named"));

    TWeakObjectPtr<TestCircle> lWeak = lFirst;
    {
        TStrongObjectPtr<TestCircle> lSecond = lFirst;
        CHECK(ObjectTable::Get().GetStrongRefCount(lHandle) == 2);

        // An owner of a parent class shares the same count.
        TStrongObjectPtr<Object> lAsObject = lSecond;
        CHECK(ObjectTable::Get().GetStrongRefCount(lHandle) == 3);
        CHECK(lAsObject.Get() == lFirst.Get());
    }
    CHECK(ObjectTable::Get().GetStrongRefCount(lHandle) == 1);

    // A move hands the owner over without counting.
    TStrongObjectPtr<Object> lMoved = Move(lFirst);
    CHECK_FALSE(lFirst.IsValid());
    CHECK(ObjectTable::Get().GetStrongRefCount(lHandle) == 1);
    CHECK(lWeak.IsValid());

    // The last owner goes: the object goes, and every handle reads null at once.
    lMoved.Reset();
    CHECK_FALSE(lWeak.IsValid());
    CHECK(lWeak.Get() == nullptr);
    CHECK(ObjectTable::Get().Resolve(lHandle) == nullptr);
    CHECK(TestCircle::s_Destroyed == lDestroyedBefore + 2);   // this one and "Named"
}

TEST_CASE("TStrongObjectPtr: an object not made by NewObject cannot be owned")
{
    TestOther lOnStack;

    const TStrongObjectPtr<TestOther> lOwner(&lOnStack);

    CHECK_FALSE(lOwner.IsValid());
    CHECK(ObjectTable::Get().GetStrongRefCount(lOnStack.GetHandle()) == 0);
}

TEST_CASE("Cast: a strong pointer down the chain shares the owner, or is empty")
{
    TStrongObjectPtr<Object> lObject = NewObject<TestRing>();

    const TStrongObjectPtr<TestCircle> lCircle = Cast<TestCircle>(lObject);
    REQUIRE(lCircle.IsValid());
    CHECK(ObjectTable::Get().GetStrongRefCount(lObject->GetHandle()) == 2);

    CHECK_FALSE(Cast<TestOther>(lObject).IsValid());
}

// =============================================================================
// Instances from the class alone
// =============================================================================

TEST_CASE("ObjectClass: instances and the default object come from the class alone")
{
    const TStrongObjectPtr<Object> lMade = NewObject(TestRing::StaticClass(), OPAAX_ID("FromClass"));
    REQUIRE(lMade.IsValid());
    CHECK(lMade->IsA<TestRing>());
    CHECK(lMade->HasAnyFlags(EObjectFlags::Managed));
    CHECK(lMade->GetName() == OPAAX_ID("FromClass"));

    CHECK_FALSE(TestShape::StaticClass().CreateInstance().IsValid());

    const Object* lDefault = TestRing::StaticClass().GetDefaultObject();
    REQUIRE(lDefault != nullptr);
    CHECK(lDefault == TestRing::StaticClass().GetDefaultObject());
    CHECK(lDefault->HasAnyFlags(EObjectFlags::ClassDefault));
    CHECK_FALSE(lDefault->HasAnyFlags(EObjectFlags::Managed));
    CHECK(lDefault->GetName() == OPAAX_ID("Default__TestRing"));
    CHECK(Cast<TestRing>(lDefault)->Thickness == 0.25f);

    CHECK(TestShape::StaticClass().GetDefaultObject() == nullptr);
}

// =============================================================================
// Properties
// =============================================================================

TEST_CASE("ObjectClass: properties are the parents' first, then the class's own")
{
    const TDynArray<const char*> lNames = TestRing::StaticClass().GetPropertyNames();
    REQUIRE(lNames.size() == 3);
    CHECK(OpaaxString(lNames[0]) == "Radius");
    CHECK(OpaaxString(lNames[1]) == "Thickness");
    CHECK(OpaaxString(lNames[2]) == "Segments");

    // A class without its own list has its parent's, once.
    CHECK(TestPlainCircle::StaticClass().GetPropertyNames().size() == 1);
    CHECK(Object::StaticClass().GetPropertyNames().empty());

    TestRing    lRing;
    NameVisitor lVisitor;
    lRing.GetClass().VisitProperties(lRing, lVisitor);

    const TDynArray<OpaaxString> lExpected = { "float Radius", "float Thickness", "int32 Segments" };
    CHECK(lVisitor.Lines == lExpected);
    CHECK(lRing.Radius == 9.f);   // written through the visitor
}

TEST_CASE("ObjectClass: properties written and read as JSON, through the whole chain")
{
    TestRing lSource;
    lSource.Radius    = 5.f;
    lSource.Thickness = 0.5f;
    lSource.Segments  = 8;

    const nlohmann::json lJson = TestRing::StaticClass().WriteProperties(lSource);
    CHECK(lJson == nlohmann::json{ { "Radius", 5.f }, { "Thickness", 0.5f }, { "Segments", 8 } });

    TestRing lCopy;
    CHECK(TestRing::StaticClass().ReadProperties(lCopy, lJson));
    CHECK(lCopy.Radius == 5.f);
    CHECK(lCopy.Thickness == 0.5f);
    CHECK(lCopy.Segments == 8);

    // A missing key keeps the field; a wrong type keeps it too and is reported by name.
    TestRing               lPartial;
    TDynArray<OpaaxString> lBad;
    CHECK(TestRing::StaticClass().ReadProperties(lPartial, nlohmann::json{ { "Radius", "big" }, { "Segments", 3 } }, &lBad));
    CHECK(lPartial.Radius == 1.f);
    CHECK(lPartial.Segments == 3);
    CHECK(lPartial.Thickness == 0.25f);
    REQUIRE(lBad.size() == 1);
    CHECK(lBad[0] == "Radius");

    CHECK_FALSE(TestRing::StaticClass().ReadProperties(lPartial, nlohmann::json::array()));

    // An object of another class is refused, not misread.
    TestOther lOther;
    CHECK(TestRing::StaticClass().WriteProperties(lOther).empty());
    CHECK_FALSE(TestRing::StaticClass().ReadProperties(lOther, lJson));
}

// =============================================================================
// References
// =============================================================================

TEST_CASE("TObjectPtr: visited with its class, collected as a reference, never saved")
{
    TStrongObjectPtr<TestCircle> lTarget = NewObject<TestCircle>();
    TStrongObjectPtr<TestCircle> lInList = NewObject<TestCircle>();
    TStrongObjectPtr<TestCircle> lNested = NewObject<TestCircle>();

    TestHolder lHolder;
    lHolder.Target       = lTarget;
    lHolder.Many         = { lInList, nullptr };
    lHolder.Group.Nested = lNested;

    CHECK(lHolder.Target.Get() == lTarget.Get());

    NameVisitor lVisitor;
    lHolder.GetClass().VisitProperties(lHolder, lVisitor);

    const TDynArray<OpaaxString> lExpected = {
        "object Target TestCircle",
        "list Many",
        "object [0] TestCircle",
        "object [1] TestCircle",
        "group Group",
        "object Nested TestCircle",
        "float Scale",
        "end",
        "float Weight",
    };
    CHECK(lVisitor.Lines == lExpected);

    // What a collector would follow: every live link, nested and listed ones included.
    const TDynArray<ObjectHandle> lReferences = lHolder.GetClass().CollectReferences(lHolder);
    REQUIRE(lReferences.size() == 3);
    CHECK(lReferences[0] == lTarget->GetHandle());
    CHECK(lReferences[1] == lInList->GetHandle());
    CHECK(lReferences[2] == lNested->GetHandle());

    // Links between live objects are not written, nor read.
    const nlohmann::json lJson = lHolder.GetClass().WriteProperties(lHolder);
    CHECK(lJson == nlohmann::json{ { "Group", { { "Scale", 9.f } } }, { "Weight", 9.f } });

    TDynArray<OpaaxString> lBad;
    CHECK(lHolder.GetClass().ReadProperties(lHolder, nlohmann::json{ { "Target", 12 } }, &lBad));
    CHECK(lBad.empty());
    CHECK(lHolder.Target.Get() == lTarget.Get());

    // Once its object is gone, a reference reads null.
    lTarget.Reset();
    CHECK(lHolder.Target.Get() == nullptr);
    CHECK_FALSE(lHolder.Target.IsValid());
    CHECK(lHolder.GetClass().CollectReferences(lHolder).size() == 3);   // the handle stays until cleared
}

TEST_CASE("TWeakObjectPtr: names an object without keeping it alive")
{
    TWeakObjectPtr<TestCircle> lWeak;
    CHECK_FALSE(lWeak.IsValid());

    {
        TStrongObjectPtr<TestRing> lRing = NewObject<TestRing>();
        lWeak = lRing;

        TWeakObjectPtr<Object> lAsObject = lWeak;
        CHECK(lAsObject.Get() == lRing.Get());
        CHECK(lWeak->Radius == 1.f);
        CHECK(ObjectTable::Get().GetStrongRefCount(lRing->GetHandle()) == 1);
    }

    CHECK_FALSE(lWeak.IsValid());
}
