// Suite: AppServiceLocator — provide/resolve by interface type, null-object fallback,
// reverse-order shutdown. The locator is header-inline so it compiles directly here;
// IPlatform is OPAAX_API-exported (StaticTypeID/Null live in the engine DLL) and is
// reached via the import lib.
//
// Two in-suite services (IServiceA/IServiceB) exercise the locator without depending on
// any real service: the OPAAX_SERVICE_TYPE macro only DECLARES StaticTypeID(), so each
// interface defines it (and its Null()) locally in this TU.
#include <doctest.h>

#include "Core/Application/Services/AppServiceLocator.h"
#include "Core/Application/Services/IAppService.h"
#include "Core/Application/Services/IPlatform.h"
#include "Core/OpaaxTypes.h"

#ifdef OPAAX_PLATFORM_WINDOWS
#include <string>
#include "Core/Application/Services/WindowsPlatform.h"
#endif

using namespace Opaax;

namespace
{
    // =============================================================================
    // In-suite services
    // =============================================================================
    class IServiceA : public IAppService
    {
    public:
        OPAAX_SERVICE_TYPE(IServiceA)
        static IServiceA& Null();
        virtual int Tag() const = 0;
    };

    class IServiceB : public IAppService
    {
    public:
        OPAAX_SERVICE_TYPE(IServiceB)
        static IServiceB& Null();
        virtual int Tag() const = 0;
    };

    //----- A impls --------------------------------------------------------------
    class NullA final : public IServiceA
    {
    public:
        bool IsNull() const noexcept override { return true; }
        int  Tag()    const override          { return -1; }
    };

    class RealA final : public IServiceA
    {
    public:
        RealA(TDynArray<int>* InRec, int InTag) : m_Rec(InRec), m_Tag(InTag) {}
        int  Tag() const override     { return m_Tag; }
        void OnShutdown() override    { if (m_Rec) { m_Rec->push_back(m_Tag); } }
    private:
        TDynArray<int>* m_Rec;
        int             m_Tag;
    };

    //----- B impls --------------------------------------------------------------
    class NullB final : public IServiceB
    {
    public:
        bool IsNull() const noexcept override { return true; }
        int  Tag()    const override          { return -1; }
    };

    class RealB final : public IServiceB
    {
    public:
        RealB(TDynArray<int>* InRec, int InTag) : m_Rec(InRec), m_Tag(InTag) {}
        int  Tag() const override     { return m_Tag; }
        void OnShutdown() override    { if (m_Rec) { m_Rec->push_back(m_Tag); } }
    private:
        TDynArray<int>* m_Rec;
        int             m_Tag;
    };

    // Out-of-line type tags + shared null objects (one each, this TU only).
    ServiceTypeID IServiceA::StaticTypeID() noexcept { static const int s_Tag = 0; return reinterpret_cast<ServiceTypeID>(&s_Tag); }
    ServiceTypeID IServiceB::StaticTypeID() noexcept { static const int s_Tag = 0; return reinterpret_cast<ServiceTypeID>(&s_Tag); }
    IServiceA& IServiceA::Null() { static NullA s_Null; return s_Null; }
    IServiceB& IServiceB::Null() { static NullB s_Null; return s_Null; }
}

// =============================================================================
// Tests
// =============================================================================
TEST_CASE("AppServiceLocator: Get returns the provided real service")
{
    AppServiceLocator lLocator;
    IServiceA& lProvided = lLocator.Provide<IServiceA, RealA>(nullptr, 7);

    CHECK_FALSE(lProvided.IsNull());
    CHECK(lLocator.Get<IServiceA>().Tag() == 7);
    CHECK(&lLocator.Get<IServiceA>() == &lProvided); // same instance, not a copy
}

TEST_CASE("AppServiceLocator: an unprovided service resolves to its null object")
{
    AppServiceLocator lLocator;
    IServiceA& lResolved = lLocator.Get<IServiceA>(); // never provided

    CHECK(lResolved.IsNull());
    CHECK(lResolved.Tag() == -1);
    CHECK(&lResolved == &IServiceA::Null());          // the shared null, never null ptr
}

TEST_CASE("AppServiceLocator: providing the same interface twice replaces it")
{
    AppServiceLocator lLocator;
    lLocator.Provide<IServiceA, RealA>(nullptr, 1);
    IServiceA& lSecond = lLocator.Provide<IServiceA, RealA>(nullptr, 2);

    CHECK(lLocator.Get<IServiceA>().Tag() == 2);
    CHECK(&lLocator.Get<IServiceA>() == &lSecond);
}

TEST_CASE("AppServiceLocator: ShutdownAll runs services in reverse provide order")
{
    TDynArray<int> lOrder;
    {
        AppServiceLocator lLocator;
        lLocator.Provide<IServiceA, RealA>(&lOrder, 1); // provided first
        lLocator.Provide<IServiceB, RealB>(&lOrder, 2); // provided last
        lLocator.ShutdownAll();                          // dtor's call is then a no-op
    }

    REQUIRE(lOrder.size() == 2u);
    CHECK(lOrder[0] == 2); // last provided -> first shut down
    CHECK(lOrder[1] == 1);
}

TEST_CASE("IPlatform: the null object is safe and self-identifying")
{
    IPlatform& lNull = IPlatform::Null();
    CHECK(lNull.IsNull());
    CHECK(lNull.GetLogicalCoreCount() == 1u);
    CHECK(lNull.GetTimeSeconds() == doctest::Approx(0.0));
    CHECK(lNull.GetExecutablePath().IsEmpty());

    // An unprovided IPlatform resolves to that same shared null — no crash.
    AppServiceLocator lLocator;
    CHECK(lLocator.Get<IPlatform>().IsNull());
    CHECK(&lLocator.Get<IPlatform>() == &IPlatform::Null());
}

#ifdef OPAAX_PLATFORM_WINDOWS
TEST_CASE("WindowsPlatform: reports real OS values through the locator")
{
    AppServiceLocator lLocator;
    IPlatform& lPlatform = lLocator.Provide<IPlatform, WindowsPlatform>();

    CHECK_FALSE(lPlatform.IsNull());
    CHECK(lPlatform.GetLogicalCoreCount() >= 1u);

    const OpaaxString lExe = lPlatform.GetExecutablePath();
    CHECK_FALSE(lExe.IsEmpty());

    const std::string lPath = lExe.CStr();
    CHECK(lPath.find(".exe") != std::string::npos);
    CHECK(lPath.find('\\') == std::string::npos); // normalised to '/'
}
#endif
