// Suite: what the clip editor does to a clip — adding steps, by hand or from a dropped texture — and
// how its preview finds and crops a sheet frame (Editor/Assets/Editors/AnimationClipEditing.h).
#include <doctest.h>

#include "Editor/Assets/Editors/AnimationClipEditing.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    SpriteFrame MakeFrame(const char* InName, const Vector2F InOffset, const Vector2F InSize)
    {
        SpriteFrame lFrame;
        lFrame.Name   = (InName[0] == '\0') ? OpaaxStringID() : OpaaxStringID(InName);
        lFrame.Offset = InOffset;
        lFrame.Size   = InSize;
        return lFrame;
    }
}

TEST_CASE("ClipEditing: Add Step copies the last step, so a run of frames is quick to author")
{
    TDynArray<AnimationStep> lSteps;

    // The first one is a default step: nothing to copy yet.
    ClipEditing::AddStep(lSteps);
    REQUIRE(lSteps.size() == 1u);
    CHECK_FALSE(lSteps[0].Frame.IsValid());
    CHECK(lSteps[0].Hold == 1u);

    lSteps[0].Frame = OPAAX_ID("Run_0");
    lSteps[0].Hold  = 3u;

    ClipEditing::AddStep(lSteps);
    REQUIRE(lSteps.size() == 2u);
    CHECK(lSteps[1].Frame == OPAAX_ID("Run_0"));
    CHECK(lSteps[1].Hold == 3u);
}

TEST_CASE("ClipEditing: a dropped texture adds a step that shows it, held as long as the last one")
{
    TDynArray<AnimationStep> lSteps;

    REQUIRE(ClipEditing::AddTextureStep(lSteps, OpaaxString("Textures/Coin_0.png")));
    REQUIRE(lSteps.size() == 1u);
    CHECK(lSteps[0].Texture.Path == "Textures/Coin_0.png");
    CHECK(lSteps[0].Hold == 1u);

    lSteps[0].Hold = 4u;
    REQUIRE(ClipEditing::AddTextureStep(lSteps, OpaaxString("Textures/Coin_1.png")));
    CHECK(lSteps[1].Texture.Path == "Textures/Coin_1.png");
    CHECK(lSteps[1].Hold == 4u);

    // A hold of zero counts as one: the copy is the hold the step actually has.
    lSteps[1].Hold = 0u;
    REQUIRE(ClipEditing::AddTextureStep(lSteps, OpaaxString("Textures/Coin_2.png")));
    CHECK(lSteps[2].Hold == 1u);

    // Nothing dropped: nothing added.
    CHECK_FALSE(ClipEditing::AddTextureStep(lSteps, OpaaxString()));
    CHECK(lSteps.size() == 3u);
}

TEST_CASE("ClipEditing: a frame is found by its name, first match, and an empty name finds none")
{
    SpriteSheetData lSheet;
    lSheet.Frames = { MakeFrame("Idle", { 0.f, 0.f }, { 32.f, 32.f }),
                      MakeFrame("", { 32.f, 0.f }, { 32.f, 32.f }),
                      MakeFrame("Run", { 64.f, 0.f }, { 32.f, 32.f }),
                      MakeFrame("Run", { 96.f, 0.f }, { 32.f, 32.f }) };

    const SpriteFrame* lRun = ClipEditing::FindFrame(lSheet, OPAAX_ID("Run"));
    REQUIRE(lRun != nullptr);
    CHECK(lRun->Offset.x == doctest::Approx(64.f));   // the first of the two, as playback binds it

    CHECK(ClipEditing::FindFrame(lSheet, OPAAX_ID("Jump")) == nullptr);
    CHECK(ClipEditing::FindFrame(lSheet, OpaaxStringID()) == nullptr);   // not the unnamed frame
}

TEST_CASE("ClipEditing: the preview crops the frame's pixels out of the image, flipped or not")
{
    // A 128x64 texture; the frame is its right half's top quarter-row.
    const SpriteFrame lFrame = MakeFrame("Run", { 64.f, 0.f }, { 64.f, 16.f });

    EditorImage lUpright;
    lUpright.Handle = 7u;

    const EditorImage lCrop = ClipEditing::CropToFrame(lUpright, lFrame, 128u, 64u);
    CHECK(lCrop.Handle == 7u);
    CHECK(lCrop.UV0.x == doctest::Approx(0.5f));
    CHECK(lCrop.UV1.x == doctest::Approx(1.f));
    CHECK(lCrop.UV0.y == doctest::Approx(0.f));
    CHECK(lCrop.UV1.y == doctest::Approx(0.25f));

    // A backend that shows its images upside down gives UVs from 1 to 0: the crop keeps that flip.
    EditorImage lFlipped;
    lFlipped.Handle = 7u;
    lFlipped.UV0    = { 0.f, 1.f };
    lFlipped.UV1    = { 1.f, 0.f };

    const EditorImage lFlippedCrop = ClipEditing::CropToFrame(lFlipped, lFrame, 128u, 64u);
    CHECK(lFlippedCrop.UV0.y == doctest::Approx(1.f));
    CHECK(lFlippedCrop.UV1.y == doctest::Approx(0.75f));

    // Nothing sensible to crop to: the whole image rather than an empty one.
    const EditorImage lNoSize = ClipEditing::CropToFrame(lUpright, MakeFrame("Bad", { 0.f, 0.f }, { 0.f, 16.f }), 128u, 64u);
    CHECK(lNoSize.UV1.x == doctest::Approx(1.f));
    CHECK(lNoSize.UV1.y == doctest::Approx(1.f));

    const EditorImage lNoTexture = ClipEditing::CropToFrame(lUpright, lFrame, 0u, 64u);
    CHECK(lNoTexture.UV0.x == doctest::Approx(0.f));
}
