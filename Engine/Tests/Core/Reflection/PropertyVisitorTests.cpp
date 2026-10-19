// Suite: IPropertyVisitor — a reflected object's fields walked without knowing its type.
#include <doctest.h>

#include "Engine/Reflection/PropertyVisitor.h"
#include "Resources/DataAsset/DataAssetRef.h"

using namespace Opaax;

namespace
{
    struct VisitorTestResource {};

    enum class EVisitorTestMode : Uint8 { Idle, Walk, Run };

    const char* ToString(const EVisitorTestMode InMode) noexcept
    {
        switch (InMode)
        {
        case EVisitorTestMode::Idle: return "Idle";
        case EVisitorTestMode::Walk: return "Walk";
        case EVisitorTestMode::Run:  return "Run";
        }
        return "?";
    }
}

OPAAX_ENUM_VALUES(EVisitorTestMode, Idle, Walk, Run)

namespace
{
    struct VisitorTestInner
    {
        float Amount = 1.f;
        float Bounded = 2.f;

        OPAAX_PROPERTIES(VisitorTestInner,
                         OPAAX_PROP(Amount),
                         OPAAX_PROP(Bounded).SetRange(-1.f, 1.f))
    };

    struct VisitorTestData
    {
        float Value = 0.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(VisitorTestData, Value)
        OPAAX_PROPERTIES(VisitorTestData, OPAAX_PROP(Value))
    };

    struct VisitorTestSubject
    {
        bool                               bEnabled = true;
        Int32                              Count    = 3;
        Vector2F                           Offset   = { 1.f, 2.f };
        LinearColor                        Tint;
        OpaaxString                        Label    = OpaaxString("hello");
        EVisitorTestMode                   Mode     = EVisitorTestMode::Walk;
        TResourcePath<VisitorTestResource> Asset;
        TDataAssetRef<VisitorTestData>     Stats;
        VisitorTestInner                   Inner;
        TDynArray<Int32>                   Numbers  = { 5, 6 };
        TDynArray<VisitorTestInner>        Items    = { VisitorTestInner{} };
        Uint64                             NotDrawable = 0;

        OPAAX_PROPERTIES(VisitorTestSubject,
                         OPAAX_PROP(bEnabled),
                         OPAAX_PROP(Count),
                         OPAAX_PROP(Offset),
                         OPAAX_PROP(Tint),
                         OPAAX_PROP(Label),
                         OPAAX_PROP(Mode),
                         OPAAX_PROP(Asset),
                         OPAAX_PROP(Stats),
                         OPAAX_PROP(Inner).SetRange(0.f, 10.f).SetDragStep(0.5f),
                         OPAAX_PROP(Numbers),
                         OPAAX_PROP(Items),
                         OPAAX_PROP(NotDrawable))
    };

    /** Writes one line per call, so the test can compare the whole walk at once. */
    class RecordingVisitor final : public IPropertyVisitor
    {
    public:
        TDynArray<OpaaxString> Lines;
        bool                   bOpenGroups = true;

        void Visit(const char* InName, bool&, const PropertyMeta&) override          { Add("bool", InName); }
        void Visit(const char* InName, Int16&, const PropertyMeta&) override         { Add("int16", InName); }
        void Visit(const char* InName, Int32& InValue, const PropertyMeta&) override
        {
            Add("int32", InName);
            InValue = 42;   // a write through the visitor
        }
        void Visit(const char* InName, Uint32&, const PropertyMeta&) override        { Add("uint32", InName); }
        void Visit(const char* InName, float&, const PropertyMeta& InMeta) override
        {
            Add("float", InName, InMeta);
        }
        void Visit(const char* InName, Vector2F&, const PropertyMeta&) override      { Add("vec2", InName); }
        void Visit(const char* InName, Vector3F&, const PropertyMeta&) override      { Add("vec3", InName); }
        void Visit(const char* InName, Vector4F&, const PropertyMeta&) override      { Add("vec4", InName); }
        void Visit(const char* InName, LinearColor&, const PropertyMeta&) override   { Add("color", InName); }
        void Visit(const char* InName, OpaaxString&, const PropertyMeta&) override   { Add("string", InName); }
        void Visit(const char* InName, OpaaxStringID&, const PropertyMeta&) override { Add("id", InName); }

        void VisitEnum(const char* InName, const char* const* InLabels, const Uint32 InCount,
                       Uint32& InOutIndex, const PropertyMeta&) override
        {
            Add("enum", InName);
            Lines.back() += OpaaxString::Format(" {} {}", InCount, InLabels[InOutIndex]);
            InOutIndex = 2;   // pick "Run"
        }

        void VisitResourcePath(const char* InName, OpaaxString&, const Uint32 InResourceTypeId,
                               const PropertyMeta&) override
        {
            Add("resource", InName);
            LastResourceTypeId = InResourceTypeId;
        }

        void VisitDataAssetRef(const char* InName, OpaaxString&, const OpaaxStringID InDataType,
                               const PropertyMeta&) override
        {
            Add("dataasset", InName);
            Lines.back() += OpaaxString::Format(" {}", InDataType.CStr());
        }

        bool BeginGroup(const char* InName, const PropertyMeta&) override
        {
            Add("group", InName);
            return bOpenGroups;
        }
        void EndGroup() override { Lines.emplace_back("end"); }

        void VisitUnsupported(const char* InName, OpaaxStringView) override { Add("unsupported", InName); }

        void VisitList(const char* InName, IPropertyList& InList, const PropertyMeta& InMeta) override
        {
            Add("list", InName);
            Lines.back() += OpaaxString::Format(" {}", InList.Count());
            IPropertyVisitor::VisitList(InName, InList, InMeta);
        }

        Uint32 LastResourceTypeId = 0;

    private:
        void Add(const char* InKind, const char* InName)
        {
            Lines.push_back(OpaaxString::Format("{} {}", InKind, InName));
        }

        void Add(const char* InKind, const char* InName, const PropertyMeta& InMeta)
        {
            Add(InKind, InName);
            Lines.back() += OpaaxString::Format(" [{},{}]", static_cast<int>(InMeta.RangeMin),
                                                static_cast<int>(InMeta.RangeMax));
        }
    };
}

TEST_CASE("PropertyVisitor: every field is visited once, in declaration order, by kind")
{
    VisitorTestSubject lSubject;
    RecordingVisitor   lVisitor;

    VisitProperties(lSubject, lVisitor);

    const TDynArray<OpaaxString> lExpected = {
        "bool bEnabled",
        "int32 Count",
        "vec2 Offset",
        "color Tint",              // its own visit, not taken for the Vector4F it derives from
        "string Label",
        "enum Mode 3 Walk",
        "resource Asset",
        "dataasset Stats VisitorTestData",   // the data type it accepts, by its saved name
        "group Inner",
        "float Amount [0,10]",     // inherits the group's range
        "float Bounded [-1,1]",    // its own range wins
        "end",
        "list Numbers 2",          // a list, then each element named by its index
        "int32 [0]",
        "int32 [1]",
        "list Items 1",
        "group [0]",
        "float Amount [0,0]",
        "float Bounded [-1,1]",
        "end",
        "unsupported NotDrawable", // reported, not dropped
    };

    CHECK(lVisitor.Lines == lExpected);
    CHECK(lVisitor.LastResourceTypeId == ResourceTypeID::Get<VisitorTestResource>());
}

TEST_CASE("PropertyVisitor: writes through the visitor land in the object")
{
    VisitorTestSubject lSubject;
    RecordingVisitor   lVisitor;

    VisitProperties(lSubject, lVisitor);

    CHECK(lSubject.Count == 42);
    CHECK(lSubject.Mode == EVisitorTestMode::Run);
    CHECK(lSubject.Numbers == TDynArray<Int32>{ 42, 42 });   // list elements too
}

TEST_CASE("PropertyVisitor: a list is edited through IPropertyList without knowing its element type")
{
    TDynArray<VisitorTestInner> lValues(2);
    lValues[0].Amount = 10.f;
    lValues[1].Amount = 20.f;

    TPropertyList<VisitorTestInner> lTyped(lValues);
    IPropertyList&                  lList = lTyped;

    lList.Add();
    CHECK(lList.Count() == 3);
    CHECK(lValues[2].Amount == 1.f);   // the element's default

    lList.Swap(0, 1);
    CHECK(lValues[0].Amount == 20.f);

    lList.RemoveAt(0);
    lList.RemoveAt(9);                 // out of range: nothing
    CHECK(lList.Count() == 2);
    CHECK(lValues[0].Amount == 10.f);

    CHECK(OpaaxString(PropertyListLabel(0).Text) == "[0]");
    CHECK(OpaaxString(PropertyListLabel(1234567).Text) == "[1234567]");
}

TEST_CASE("PropertyVisitor: a group the visitor declines is skipped, with no EndGroup")
{
    VisitorTestSubject lSubject;
    RecordingVisitor   lVisitor;
    lVisitor.bOpenGroups = false;

    VisitProperties(lSubject, lVisitor);

    for (const OpaaxString& lLine : lVisitor.Lines)
    {
        CHECK_FALSE(lLine.Contains("Amount"));
        CHECK(lLine != "end");
    }
}
