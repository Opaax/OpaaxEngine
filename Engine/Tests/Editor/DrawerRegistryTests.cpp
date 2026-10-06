// Suite: TDrawerRegistry bookkeeping (Editor/Extensions/DrawerRegistry.h): which component types are
// already covered, so the editor only adds its generic drawer where nothing else draws. Header-only:
// nothing here draws (that lives in OpaaxEditorLib, not linked).
#include <doctest.h>

#include "Editor/Extensions/DrawerRegistry.h"
#include "Renderer/Components/QuadComponent.h"
#include "World/Components/TransformComponent.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    struct StubQuadDrawer
    {
        void Draw(IEditorWidgets&, QuadComponent&) {}
    };
}

TEST_CASE("DrawerRegistry: HasTarget knows typed and erased registrations, and nothing else")
{
    ComponentDrawerRegistry lDrawers;

    CHECK_FALSE(lDrawers.HasTarget(TypeIdOf<QuadComponent>()));

    lDrawers.Register<QuadComponent, StubQuadDrawer>();
    CHECK(lDrawers.HasTarget(TypeIdOf<QuadComponent>()));

    // An erased entry names its target by id: the route the generic component drawer takes.
    const TypeId lTransformId = TypeIdOf<TransformComponent>();
    CHECK_FALSE(lDrawers.HasTarget(lTransformId));

    lDrawers.RegisterErased(lTransformId, [](Entity&, IEditorWidgets&, EditorContext&) { return false; });
    CHECK(lDrawers.HasTarget(lTransformId));

    CHECK(lDrawers.Count() == 2u);
}
