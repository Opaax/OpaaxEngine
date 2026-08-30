// Suite: the gizmo's delta math (Editor/Operation/EditorGizmo.hpp).
//
// THE FIRST TEST TO REACH EDITOR CODE, and it is here because of a bug the user found by eye:
// scaling an entity that was not at the world origin flung it and compounded every frame. The cause
// was trusting ImGuizmo's `deltaMatrix`, which is a per-frame increment for translate and rotate but
// a CUMULATIVE, ORIGIN-CENTRED scale for scale. At (0,0) that is invisible.
//
// EditorGizmo is header-only and touches no ImGui, so this needs the editor's include dir and no
// link. What is pinned is the one expression the fix rests on — delta = M * inverse(M last frame),
// with the matrix sitting ON THE PIVOT so the conjugation is free.
#include <doctest.h>

#include <cmath>
#include <glm/mat2x2.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

#include "Editor/Operation/EditorGizmo.hpp"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    /** Where a banked delta sends a world point — what EntityOps::TransformSelected does per entity. */
    Vector2F Apply(const Matrix44F& InDelta, const Vector2F& InPoint)
    {
        const Vector4F lMoved = InDelta * Vector4F(InPoint.x, InPoint.y, 0.f, 1.f);

        return { lMoved.x, lMoved.y };
    }

    /** The scale factor EntityOps reads off the delta's own basis. */
    Vector2F BasisScale(const Matrix44F& InDelta)
    {
        return { glm::length(Vector2F{ InDelta[0][0], InDelta[0][1] }),
                 glm::length(Vector2F{ InDelta[1][0], InDelta[1][1] }) };
    }

    /** Stand in for ImGuizmo: scale the gizmo's matrix about ITS OWN origin, as Manipulate does. */
    void ScaleMatrixInPlace(Matrix44F& InOutMatrix, const Vector2F& InFactor)
    {
        InOutMatrix[0] *= InFactor.x;
        InOutMatrix[1] *= InFactor.y;
    }

    /**
     * What EntityOps::TransformSelected reads for an entity turned by InDegrees — the delta's linear
     * part conjugated into that entity's own frame. Mirrored here rather than shared because
     * EntityOps lives in a .cpp that links OpaaxEditorLib, which these tests deliberately do not.
     */
    Vector2F LocalScaleOf(const Matrix44F& InDelta, const float InDegrees)
    {
        const glm::mat2 lLinear{ Vector2F{ InDelta[0][0], InDelta[0][1] },
                                 Vector2F{ InDelta[1][0], InDelta[1][1] } };

        if (InDegrees == 0.f) { return { glm::length(lLinear[0]), glm::length(lLinear[1]) }; }

        const float     lRad = glm::radians(InDegrees);
        const glm::mat2 lRot{ Vector2F{ std::cos(lRad), std::sin(lRad) },
                              Vector2F{ -std::sin(lRad), std::cos(lRad) } };

        const glm::mat2 lLocal = glm::transpose(lRot) * lLinear * lRot;

        return { glm::length(lLocal[0]), glm::length(lLocal[1]) };
    }

    /** The spurious turn the same reading reports — must be zero for a pure local scale. */
    float LocalRotationOf(const Matrix44F& InDelta, const float InDegrees)
    {
        const glm::mat2 lLinear{ Vector2F{ InDelta[0][0], InDelta[0][1] },
                                 Vector2F{ InDelta[1][0], InDelta[1][1] } };

        if (InDegrees == 0.f) { return glm::degrees(std::atan2(lLinear[0][1], lLinear[0][0])); }

        const float     lRad = glm::radians(InDegrees);
        const glm::mat2 lRot{ Vector2F{ std::cos(lRad), std::sin(lRad) },
                              Vector2F{ -std::sin(lRad), std::cos(lRad) } };

        const glm::mat2 lLocal = glm::transpose(lRot) * lLinear * lRot;

        return glm::degrees(std::atan2(lLocal[0][1], lLocal[0][0]));
    }
}

TEST_CASE("EditorGizmo: a scale delta leaves the pivot itself untouched")
{
    // The single-selection case: the pivot IS the entity, so scaling must change its size and NOT
    // move it. This is the assertion that fails against an origin-centred scale for any pivot but
    // (0,0) — which is exactly how the bug presented.
    EditorGizmo    lGizmo;
    const Vector2F lPivot{ 400.f, -250.f };

    lGizmo.ReseatAt(lPivot, 0.f);
    ScaleMatrixInPlace(lGizmo.Matrix(), { 2.f, 2.f });
    lGizmo.BankFrameDelta();

    const Matrix44F lDelta = lGizmo.ConsumeDelta();
    const Vector2F  lMoved = Apply(lDelta, lPivot);

    CHECK(lMoved.x == doctest::Approx(lPivot.x));
    CHECK(lMoved.y == doctest::Approx(lPivot.y));

    CHECK(BasisScale(lDelta).x == doctest::Approx(2.f));
    CHECK(BasisScale(lDelta).y == doctest::Approx(2.f));
}

TEST_CASE("EditorGizmo: a scale delta scales OTHER points about the pivot, not the origin")
{
    // The multi-selection case. A point 100 units right of the pivot must end 200 units right of it
    // — not at twice its WORLD coordinate, which is what an origin-centred scale would give.
    EditorGizmo    lGizmo;
    const Vector2F lPivot{ 1000.f, 0.f };

    lGizmo.ReseatAt(lPivot, 0.f);
    ScaleMatrixInPlace(lGizmo.Matrix(), { 2.f, 2.f });
    lGizmo.BankFrameDelta();

    const Vector2F lMoved = Apply(lGizmo.ConsumeDelta(), { 1100.f, 0.f });

    CHECK(lMoved.x == doctest::Approx(1200.f));   // origin-centred would answer 2200
    CHECK(lMoved.y == doctest::Approx(0.f));
}

TEST_CASE("EditorGizmo: consecutive frames bank INCREMENTS, never the cumulative factor")
{
    // The second half of the same bug. ImGuizmo's scale delta is measured from the DRAG START, so
    // reading it every frame and multiplying compounds: 1.5 then 2.0 would apply 3x. Taking the
    // delta from our own matrix cannot do that, because the previous frame is subtracted out.
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 0.f, 0.f }, 0.f);

    ScaleMatrixInPlace(lGizmo.Matrix(), { 1.5f, 1.5f });
    lGizmo.BankFrameDelta();
    CHECK(BasisScale(lGizmo.ConsumeDelta()).x == doctest::Approx(1.5f));

    // The matrix is now at 1.5x total. One more frame taking it to 2x total is a 4/3 INCREMENT.
    ScaleMatrixInPlace(lGizmo.Matrix(), { 2.f / 1.5f, 2.f / 1.5f });
    lGizmo.BankFrameDelta();
    CHECK(BasisScale(lGizmo.ConsumeDelta()).x == doctest::Approx(2.f / 1.5f));
}

TEST_CASE("EditorGizmo: banked deltas COMPOSE while unspent")
{
    // ApplyGizmoDrag runs once per frame, but nothing guarantees it: the panel measures in the ImGui
    // pass and spends in OnPreRender, so a skipped frame must lose no motion.
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 0.f, 0.f }, 0.f);

    ScaleMatrixInPlace(lGizmo.Matrix(), { 2.f, 2.f });
    lGizmo.BankFrameDelta();

    ScaleMatrixInPlace(lGizmo.Matrix(), { 3.f, 3.f });
    lGizmo.BankFrameDelta();

    CHECK(BasisScale(lGizmo.ConsumeDelta()).x == doctest::Approx(6.f));
}

TEST_CASE("EditorGizmo: a translate delta is a pure translation, wherever the pivot is")
{
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 700.f, 300.f }, 0.f);

    lGizmo.Matrix()[3][0] += 25.f;
    lGizmo.Matrix()[3][1] -= 10.f;
    lGizmo.BankFrameDelta();

    const Matrix44F lDelta = lGizmo.ConsumeDelta();

    // Every point moves by the same amount, and nothing is scaled — the property that makes a
    // multi-selection keep its layout.
    const Vector2F lNear = Apply(lDelta, { 700.f, 300.f });
    const Vector2F lFar  = Apply(lDelta, { -900.f, 50.f });

    CHECK(lNear.x == doctest::Approx(725.f));
    CHECK(lNear.y == doctest::Approx(290.f));
    CHECK(lFar.x  == doctest::Approx(-875.f));
    CHECK(lFar.y  == doctest::Approx(40.f));
    CHECK(BasisScale(lDelta).x == doctest::Approx(1.f));
}

TEST_CASE("EditorGizmo: ConsumeDelta clears, so an unspent frame cannot be applied twice")
{
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 0.f, 0.f }, 0.f);
    ScaleMatrixInPlace(lGizmo.Matrix(), { 4.f, 4.f });
    lGizmo.BankFrameDelta();

    REQUIRE(lGizmo.HasPendingDelta());
    CHECK(BasisScale(lGizmo.ConsumeDelta()).x == doctest::Approx(4.f));

    CHECK_FALSE(lGizmo.HasPendingDelta());
    CHECK(BasisScale(lGizmo.ConsumeDelta()).x == doctest::Approx(1.f));   // identity, not 4x again
}

// =============================================================================
// ③b — pivot and space
// =============================================================================
TEST_CASE("EditorGizmo: ReseatAt with a rotation is what makes Local differ from World")
{
    // ③ always built this matrix with an identity rotation, so the two spaces would have drawn
    // identically. The X axis of a gizmo seated at 90 degrees must point along world +Y.
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 0.f, 0.f }, glm::radians(90.f));

    const Matrix44F& lMatrix = lGizmo.Matrix();

    CHECK(lMatrix[0][0] == doctest::Approx(0.f).epsilon(0.001));
    CHECK(lMatrix[0][1] == doctest::Approx(1.f));
    CHECK(lMatrix[1][0] == doctest::Approx(-1.f));
    CHECK(lMatrix[1][1] == doctest::Approx(0.f).epsilon(0.001));

    // Unit scale regardless: the matrix measures a DRAG, not the entity. Seeding it with the
    // entity's scale would make the first frame's delta report a stretch nobody applied.
    CHECK(glm::length(Vector2F{ lMatrix[0][0], lMatrix[0][1] }) == doctest::Approx(1.f));
}

TEST_CASE("EditorGizmo: a LOCAL scale of a rotated entity reads back clean in its own frame")
{
    // THE CASE THE CONJUGATION EXISTS FOR. A 45-degree entity scaled 2x along its own X arrives as
    // R*S*R-inverse; read against WORLD axes that is ~1.58x on both axes plus ~18 degrees of
    // rotation nobody asked for. Read in the entity's frame it is exactly 2x, 1x, and no turn.
    EditorGizmo lGizmo;
    const float lAngle = 45.f;

    lGizmo.ReseatAt({ 300.f, -120.f }, glm::radians(lAngle));
    ScaleMatrixInPlace(lGizmo.Matrix(), { 2.f, 1.f });
    lGizmo.BankFrameDelta();

    const Matrix44F lDelta = lGizmo.ConsumeDelta();

    const Vector2F lLocal = LocalScaleOf(lDelta, lAngle);
    CHECK(lLocal.x == doctest::Approx(2.f));
    CHECK(lLocal.y == doctest::Approx(1.f));
    CHECK(LocalRotationOf(lDelta, lAngle) == doctest::Approx(0.f).epsilon(0.001));

    // And the world reading really is the wrong one — if these agreed the conjugation would be
    // pointless and this test would pass against the broken code.
    CHECK(LocalScaleOf(lDelta, 0.f).x == doctest::Approx(1.5811f).epsilon(0.01));
    CHECK(LocalRotationOf(lDelta, 0.f) == doctest::Approx(18.435f).epsilon(0.01));
}

TEST_CASE("EditorGizmo: the pivot still holds still under a LOCAL scale")
{
    EditorGizmo    lGizmo;
    const Vector2F lPivot{ -800.f, 640.f };

    lGizmo.ReseatAt(lPivot, glm::radians(30.f));
    ScaleMatrixInPlace(lGizmo.Matrix(), { 3.f, 0.5f });
    lGizmo.BankFrameDelta();

    const Vector2F lMoved = Apply(lGizmo.ConsumeDelta(), lPivot);

    CHECK(lMoved.x == doctest::Approx(lPivot.x));
    CHECK(lMoved.y == doctest::Approx(lPivot.y));
}

TEST_CASE("EditorGizmo: a rotation delta is unaffected by which frame it is read in")
{
    // 2D rotations commute, so conjugating one by the entity's own rotation changes nothing. That
    // is what lets ONE code path serve all three modes instead of branching on the mode.
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 0.f, 0.f }, glm::radians(20.f));

    // Turn the gizmo a further 10 degrees, as a rotate drag would.
    const float lNew = glm::radians(30.f);
    lGizmo.Matrix()[0] = Vector4F{ std::cos(lNew), std::sin(lNew), 0.f, 0.f };
    lGizmo.Matrix()[1] = Vector4F{ -std::sin(lNew), std::cos(lNew), 0.f, 0.f };
    lGizmo.BankFrameDelta();

    const Matrix44F lDelta = lGizmo.ConsumeDelta();

    CHECK(LocalRotationOf(lDelta, 20.f) == doctest::Approx(10.f));
    CHECK(LocalRotationOf(lDelta, 0.f)  == doctest::Approx(10.f));   // same answer either way
    CHECK(LocalScaleOf(lDelta, 20.f).x  == doctest::Approx(1.f));
}

TEST_CASE("EditorGizmo: an UNROTATED entity is bit-identical to the pre-③b path")
{
    // The conjugation early-outs at zero degrees, so the overwhelmingly common case must not have
    // moved at all.
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 50.f, 50.f }, 0.f);
    ScaleMatrixInPlace(lGizmo.Matrix(), { 2.5f, 0.5f });
    lGizmo.BankFrameDelta();

    const Matrix44F lDelta = lGizmo.ConsumeDelta();

    CHECK(LocalScaleOf(lDelta, 0.f).x == doctest::Approx(2.5f));
    CHECK(LocalScaleOf(lDelta, 0.f).y == doctest::Approx(0.5f));
    CHECK(LocalRotationOf(lDelta, 0.f) == doctest::Approx(0.f));
}

TEST_CASE("EditorGizmo: a scale reads clean in the GIZMO's frame, for every entity in the selection")
{
    // THE MULTI-SELECT BUG, pinned. The delta is R*S*R-inverse where R is the pose the gizmo was
    // SEATED with. Conjugating by that R recovers S for everyone; conjugating by each entity's own
    // rotation only cancels for the entity that happens to match, and every other one picks up a
    // rotation nobody asked for. The gizmo remembers its frame precisely so this cannot be guessed.
    EditorGizmo lGizmo;
    const float lGizmoAngle = 40.f;

    lGizmo.ReseatAt({ 0.f, 0.f }, glm::radians(lGizmoAngle));
    REQUIRE(lGizmo.GetFrameRad() == doctest::Approx(glm::radians(lGizmoAngle)));

    ScaleMatrixInPlace(lGizmo.Matrix(), { 2.f, 1.f });
    lGizmo.BankFrameDelta();

    const Matrix44F lDelta = lGizmo.ConsumeDelta();

    // Read in the GIZMO's frame — exact, and no turn.
    CHECK(LocalScaleOf(lDelta, lGizmoAngle).x    == doctest::Approx(2.f));
    CHECK(LocalScaleOf(lDelta, lGizmoAngle).y    == doctest::Approx(1.f));
    CHECK(LocalRotationOf(lDelta, lGizmoAngle)   == doctest::Approx(0.f).epsilon(0.001));

    // Read in some OTHER entity's frame — the reading the bug used. Both wrong, and the rotation is
    // the visible symptom: a selected entity at a different angle crept round as it was scaled.
    const float lOtherAngle = 0.f;
    CHECK(LocalRotationOf(lDelta, lOtherAngle) != doctest::Approx(0.f).epsilon(0.001));
    CHECK(LocalScaleOf(lDelta, lOtherAngle).x  != doctest::Approx(2.f).epsilon(0.001));
}

TEST_CASE("EditorGizmo: the frame survives a drag, because a drag never reseats")
{
    // ApplyGizmoDrag reads GetFrameRad() a frame after MeasureGizmo banked the delta. If the frame
    // were re-derived from the selection at apply time it could answer differently mid-gesture and
    // the scale would decode against the wrong R.
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 10.f, 10.f }, glm::radians(25.f));

    ScaleMatrixInPlace(lGizmo.Matrix(), { 1.5f, 1.5f });
    lGizmo.BankFrameDelta();
    ScaleMatrixInPlace(lGizmo.Matrix(), { 1.5f, 1.5f });
    lGizmo.BankFrameDelta();

    CHECK(lGizmo.GetFrameRad() == doctest::Approx(glm::radians(25.f)));
    CHECK(LocalScaleOf(lGizmo.ConsumeDelta(), 25.f).x == doctest::Approx(2.25f));
}

TEST_CASE("EditorGizmo: Individual origins is refused for a TRANSLATE, whatever the pivot says")
{
    // "About its own origin" has no meaning for a translation — everything moves by the same
    // offset. Stated on the gizmo so the toolbar's label and the mutation cannot disagree.
    EditorGizmo lGizmo;

    lGizmo.SetPivot(EGizmoPivot::Individual);

    lGizmo.SetMode(EGizmoMode::Translate);
    CHECK_FALSE(lGizmo.UsesIndividualOrigins());

    lGizmo.SetMode(EGizmoMode::Rotate);
    CHECK(lGizmo.UsesIndividualOrigins());

    lGizmo.SetMode(EGizmoMode::Scale);
    CHECK(lGizmo.UsesIndividualOrigins());

    // And it is the PIVOT that enables it, not the mode alone.
    lGizmo.SetPivot(EGizmoPivot::Center);
    CHECK_FALSE(lGizmo.UsesIndividualOrigins());
}

TEST_CASE("EditorGizmo: a SHARED-pivot rotation orbits, which is what Individual must not do")
{
    // The distinction the user asked for, pinned from the delta's side: a rotation about a shared
    // pivot MOVES an entity that is not at that pivot. EntityOps' Individual branch is exactly the
    // choice to discard this displacement and keep only the turn.
    EditorGizmo    lGizmo;
    const Vector2F lPivot{ 0.f, 0.f };

    lGizmo.ReseatAt(lPivot, 0.f);

    const float lNew = glm::radians(90.f);
    lGizmo.Matrix()[0] = Vector4F{ std::cos(lNew), std::sin(lNew), 0.f, 0.f };
    lGizmo.Matrix()[1] = Vector4F{ -std::sin(lNew), std::cos(lNew), 0.f, 0.f };
    lGizmo.BankFrameDelta();

    const Matrix44F lDelta = lGizmo.ConsumeDelta();

    // An entity 100 to the right of the pivot swings to 100 ABOVE it — a real displacement, and
    // the whole reason "rotate them as a group" and "rotate each of them" are different verbs.
    const Vector2F lOrbited = Apply(lDelta, { 100.f, 0.f });
    CHECK(lOrbited.x == doctest::Approx(0.f).epsilon(0.001));
    CHECK(lOrbited.y == doctest::Approx(100.f));

    // The TURN is the same either way — Individual keeps this and drops the displacement above.
    CHECK(LocalRotationOf(lDelta, 0.f) == doctest::Approx(90.f));
}

TEST_CASE("EditorGizmo: ReseatAt re-anchors BOTH matrices, so idling banks nothing")
{
    // The gizmo follows the selection every frame it is not being dragged. If ReseatAt moved only
    // the live matrix, the next real drag would difference against a stale pose and jump.
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 0.f, 0.f }, 0.f);
    lGizmo.ReseatAt({ 500.f, 500.f }, 0.f);   // the selection moved, or another entity was picked
    lGizmo.BankFrameDelta();

    const Matrix44F lDelta = lGizmo.ConsumeDelta();
    const Vector2F  lMoved = Apply(lDelta, { 123.f, -456.f });

    CHECK(lMoved.x == doctest::Approx(123.f));
    CHECK(lMoved.y == doctest::Approx(-456.f));
}
