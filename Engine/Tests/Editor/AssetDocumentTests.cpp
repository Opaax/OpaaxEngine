// Suite: AssetDocument (Editor/Assets/AssetDocument.hpp) — an asset open in the editor: its own copy,
// the dirty mark, saving, all-or-nothing value changes, a class no module registered, and a file of
// its class's own format (read-only).
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/Asset.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace AssetDocumentTestTypes
{
    class DocTuning final : public Asset
    {
        OPAAX_OBJECT(DocTuning, Asset)

    public:
        float       Speed = 1.f;
        Int32       Lives = 3;
        OpaaxString Title = OpaaxString("Hero");

        /** Derived on load. */
        Int32 Loads = 0;

        OPAAX_PROPERTIES(DocTuning, OPAAX_PROP(Speed), OPAAX_PROP(Lives), OPAAX_PROP(Title))

        void PostLoad() override { ++Loads; }
    };

    class DocAbstract : public Asset
    {
        OPAAX_ABSTRACT_OBJECT(DocAbstract, Asset)
    };

    // A note read from a text file of its own.
    class DocNote final : public Asset
    {
        OPAAX_OBJECT(DocNote, Asset)
        OPAAX_ASSET_SOURCE(".docnote")

    public:
        OpaaxString Text;
        Int32       Initializations = 0;

        //~Begin Asset interface
        bool LoadSource(const char* InAbsPath) override
        {
            Text = FileIO::ReadAllText(OpaaxString(InAbsPath));
            return !Text.IsEmpty();
        }

        void InitializeOnMainThread() override { ++Initializations; }
        //~End Asset interface
    };
}

using namespace AssetDocumentTestTypes;

namespace
{
    struct Fixture
    {
        TestTempDir         Dir{ "AssetDocument" };
        ObjectClassRegistry Classes;

        Fixture()
        {
            Classes.Register<DocTuning>();
            Classes.Register<DocNote>();
        }

        OpaaxString Write(const OpaaxStringView InName, const char* InText) const
        {
            const OpaaxString lPath = Dir.Sub(InName);
            FileIO::WriteAllText(lPath, OpaaxString(InText));
            return lPath;
        }
    };
}

TEST_CASE("AssetDocument: opened clean; an edit marks it dirty; Save writes it and clears the mark")
{
    Fixture lFixture;
    const OpaaxString lPath = lFixture.Write("Hero.opaaxasset",
                                             R"({ "Class": "DocTuning", "Properties": { "Speed": 2.0 } })");

    OpaaxString               lError;
    TUniquePtr<AssetDocument> lDocument = AssetDocument::Open(lPath, lFixture.Classes, lError);
    REQUIRE(lDocument != nullptr);

    CHECK(lDocument->GetFileName() == "Hero.opaaxasset");
    CHECK(lDocument->GetClassName() == OPAAX_ID("DocTuning"));
    REQUIRE(lDocument->IsClassKnown());
    CHECK(lDocument->GetAsset()->GetName() == OPAAX_ID("Hero"));
    CHECK_FALSE(lDocument->IsDirty());

    DocTuning* lTuning = Cast<DocTuning>(lDocument->GetAsset());
    REQUIRE(lTuning != nullptr);
    CHECK(lTuning->Speed == 2.f);

    // Edited the way a property widget does it: straight into the copy.
    lTuning->Lives = 9;
    CHECK(lDocument->IsDirty());

    REQUIRE(lDocument->Save());
    CHECK_FALSE(lDocument->IsDirty());

    // The file holds every property now, in the engine's form.
    TUniquePtr<AssetDocument> lReopened = AssetDocument::Open(lPath, lFixture.Classes, lError);
    REQUIRE(lReopened != nullptr);
    CHECK(Cast<DocTuning>(lReopened->GetAsset())->Lives == 9);
    CHECK(FileIO::ReadAllText(lPath) == lDocument->Serialize());
}

TEST_CASE("AssetDocument: values replaced all or nothing, then PostLoad")
{
    Fixture lFixture;
    const OpaaxString lPath = lFixture.Write("Hero.opaaxasset", R"({ "Class": "DocTuning" })");

    OpaaxString               lError;
    TUniquePtr<AssetDocument> lDocument = AssetDocument::Open(lPath, lFixture.Classes, lError);
    REQUIRE(lDocument != nullptr);

    DocTuning* lTuning = Cast<DocTuning>(lDocument->GetAsset());
    const Int32 lLoadsBefore = lTuning->Loads;

    REQUIRE(lDocument->SetProperties(nlohmann::json{ { "Speed", 5.f }, { "Title", "Boss" } }));
    CHECK(lTuning->Speed == 5.f);
    CHECK(lTuning->Title == "Boss");
    CHECK(lTuning->Lives == 3);                   // not named: kept
    CHECK(lTuning->Loads == lLoadsBefore + 1);    // PostLoad ran

    // One bad value: nothing changes, the field is named.
    CHECK_FALSE(lDocument->SetProperties(nlohmann::json{ { "Speed", 7.f }, { "Lives", "many" } }, &lError));
    CHECK(lTuning->Speed == 5.f);
    CHECK(lTuning->Lives == 3);
    CHECK(lError.Contains("Lives"));

    CHECK_FALSE(lDocument->SetProperties(nlohmann::json::array()));
}

TEST_CASE("AssetDocument: a class no module registered is shown read-only, never rewritten")
{
    Fixture lFixture;
    const OpaaxString lPath = lFixture.Write("Alien.opaaxasset",
                                             R"({ "Class": "NobodyRegisteredThis", "Properties": { "Legs": 6 } })");

    OpaaxString               lError;
    TUniquePtr<AssetDocument> lDocument = AssetDocument::Open(lPath, lFixture.Classes, lError);
    REQUIRE(lDocument != nullptr);

    CHECK_FALSE(lDocument->IsClassKnown());
    CHECK(lDocument->GetAsset() == nullptr);
    CHECK(lDocument->GetProperties() == nlohmann::json{ { "Legs", 6 } });
    CHECK_FALSE(lDocument->IsDirty());

    const OpaaxString lBefore = FileIO::ReadAllText(lPath);
    CHECK_FALSE(lDocument->Save());
    CHECK_FALSE(lDocument->SetProperties(nlohmann::json{ { "Legs", 8 } }));
    CHECK(FileIO::ReadAllText(lPath) == lBefore);
}

TEST_CASE("AssetDocument: a file that is not an asset, or a value of the wrong type, does not open")
{
    Fixture     lFixture;
    OpaaxString lError;

    CHECK(AssetDocument::Open(lFixture.Write("Broken.opaaxasset", "{"), lFixture.Classes, lError) == nullptr);
    CHECK_FALSE(lError.IsEmpty());

    lError = OpaaxString();
    CHECK(AssetDocument::Open(lFixture.Write("Wrong.opaaxasset", R"({ "Class": "DocTuning", "Properties": { "Lives": "x" } })"),
                              lFixture.Classes, lError) == nullptr);
    CHECK(lError.Contains("Lives"));

    CHECK(AssetDocument::Open(lFixture.Dir.Sub("Missing.opaaxasset"), lFixture.Classes, lError) == nullptr);
}

TEST_CASE("AssetDocument: a new asset has its class's defaults and stays dirty until saved")
{
    Fixture lFixture;
    const OpaaxString lPath = lFixture.Dir.Sub("NewDocTuning.opaaxasset");

    TUniquePtr<AssetDocument> lDocument = AssetDocument::Create(lPath, DocTuning::StaticClass());
    REQUIRE(lDocument != nullptr);
    CHECK(lDocument->IsDirty());
    CHECK(lDocument->GetAsset()->GetName() == OPAAX_ID("NewDocTuning"));
    CHECK(lDocument->GetProperties() == nlohmann::json{ { "Lives", 3 }, { "Speed", 1.f }, { "Title", "Hero" } });

    REQUIRE(lDocument->Save());
    CHECK_FALSE(lDocument->IsDirty());

    // Only a concrete Asset class can be made.
    CHECK(AssetDocument::Create(lPath, DocAbstract::StaticClass()) == nullptr);
    CHECK(AssetDocument::Create(lPath, Object::StaticClass()) == nullptr);
}

TEST_CASE("AssetDocument: a file of its class's own format opens read-only: never dirty, written or edited")
{
    Fixture lFixture;
    const OpaaxString lPath = lFixture.Write("Memo.docnote", "remember");

    OpaaxString               lError;
    TUniquePtr<AssetDocument> lDocument = AssetDocument::Open(lPath, lFixture.Classes, lError);
    REQUIRE(lDocument != nullptr);

    CHECK(lDocument->IsFromSourceFile());
    CHECK(lDocument->GetClassName() == OPAAX_ID("DocNote"));

    const DocNote* lNote = Cast<DocNote>(lDocument->GetAsset());
    REQUIRE(lNote != nullptr);
    CHECK(lNote->Text == "remember");
    CHECK(lNote->Initializations == 1);   // opened on the main thread

    CHECK_FALSE(lDocument->IsDirty());
    CHECK_FALSE(lDocument->Save());
    CHECK(FileIO::ReadAllText(lPath) == "remember");

    OpaaxString lEditError;
    CHECK_FALSE(lDocument->SetProperties(nlohmann::json::object(), &lEditError));
    CHECK(lEditError.Contains("own file"));
}

TEST_CASE("AssetDocument: a file no class reads, or that its class cannot read, does not open")
{
    Fixture lFixture;

    OpaaxString lError;
    CHECK(AssetDocument::Open(lFixture.Write("Data.nobody", "x"), lFixture.Classes, lError) == nullptr);
    CHECK(lError.Contains(".nobody"));

    lError = OpaaxString();
    CHECK(AssetDocument::Open(lFixture.Write("Blank.docnote", ""), lFixture.Classes, lError) == nullptr);
    CHECK(lError.Contains("DocNote"));
}

TEST_CASE("AssetDocument: a class read from files of its own is never made as a new asset")
{
    Fixture lFixture;
    CHECK(AssetDocument::Create(lFixture.Dir.Sub("New.opaaxasset"), DocNote::StaticClass()) == nullptr);
}
