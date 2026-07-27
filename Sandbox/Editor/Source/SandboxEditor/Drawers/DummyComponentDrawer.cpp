#include "Drawers/DummyComponentDrawer.h"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>   // value_ptr — the codebase's glm<->float* idiom (Renderer2D.cpp)

void DummyComponentDrawer::Draw(Opaax::DummyComponent& InComponent) const
{
    // The drawer owns its own presentation, header included — which is why the registry needs no display
    // name and the M0 call-site shape Register<TComponent, TDrawer>() survived unchanged.
    if (!ImGui::CollapsingHeader("Dummy Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::DragFloat2("Position", glm::value_ptr(InComponent.Position), 1.f);
    ImGui::DragFloat2("Size",     glm::value_ptr(InComponent.Size),     1.f, 1.f, 4096.f);
    ImGui::ColorEdit4("Color",    glm::value_ptr(InComponent.Color));
}
