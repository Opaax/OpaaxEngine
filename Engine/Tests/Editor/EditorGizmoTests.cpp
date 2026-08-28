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
}

TEST_CASE("EditorGizmo: a scale delta leaves the pivot itself untouched")
{
    // The single-selection case: the pivot IS the entity, so scaling must change its size and NOT
    // move it. This is the assertion that fails against an origin-centred scale for any pivot but
    // (0,0) — which is exactly how the bug presented.
    EditorGizmo    lGizmo;
    const Vector2F lPivot{ 400.f, -250.f };

    lGizmo.ReseatAt(lPivot);
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

    lGizmo.ReseatAt(lPivot);
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

    lGizmo.ReseatAt({ 0.f, 0.f });

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

    lGizmo.ReseatAt({ 0.f, 0.f });

    ScaleMatrixInPlace(lGizmo.Matrix(), { 2.f, 2.f });
    lGizmo.BankFrameDelta();

    ScaleMatrixInPlace(lGizmo.Matrix(), { 3.f, 3.f });
    lGizmo.BankFrameDelta();

    CHECK(BasisScale(lGizmo.ConsumeDelta()).x == doctest::Approx(6.f));
}

TEST_CASE("EditorGizmo: a translate delta is a pure translation, wherever the pivot is")
{
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 700.f, 300.f });

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

    lGizmo.ReseatAt({ 0.f, 0.f });
    ScaleMatrixInPlace(lGizmo.Matrix(), { 4.f, 4.f });
    lGizmo.BankFrameDelta();

    REQUIRE(lGizmo.HasPendingDelta());
    CHECK(BasisScale(lGizmo.ConsumeDelta()).x == doctest::Approx(4.f));

    CHECK_FALSE(lGizmo.HasPendingDelta());
    CHECK(BasisScale(lGizmo.ConsumeDelta()).x == doctest::Approx(1.f));   // identity, not 4x again
}

TEST_CASE("EditorGizmo: ReseatAt re-anchors BOTH matrices, so idling banks nothing")
{
    // The gizmo follows the selection every frame it is not being dragged. If ReseatAt moved only
    // the live matrix, the next real drag would difference against a stale pose and jump.
    EditorGizmo lGizmo;

    lGizmo.ReseatAt({ 0.f, 0.f });
    lGizmo.ReseatAt({ 500.f, 500.f });   // the selection moved, or another entity was picked
    lGizmo.BankFrameDelta();

    const Matrix44F lDelta = lGizmo.ConsumeDelta();
    const Vector2F  lMoved = Apply(lDelta, { 123.f, -456.f });

    CHECK(lMoved.x == doctest::Approx(123.f));
    CHECK(lMoved.y == doctest::Approx(-456.f));
}
