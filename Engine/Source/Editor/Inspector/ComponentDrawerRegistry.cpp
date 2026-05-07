#include "ComponentDrawerRegistry.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

namespace Opaax::Editor
{
    void ComponentDrawerRegistry::DrawAll(World& InWorld, EntityID InEntity)
    {
        for (auto& lDrawer : GetStorage())
        {
            if (lDrawer->HasComponent(InWorld, InEntity))
            {
                lDrawer->Draw(InWorld, InEntity);
            }
        }
    }

    void ComponentDrawerRegistry::DrawAddComponentMenu(World& InWorld, EntityID InEntity)
    {
        for (auto& lDrawer : GetStorage())
        {
            if (!lDrawer->CanAdd())
            {
                continue;
            }

            if (lDrawer->HasComponent(InWorld, InEntity))
            {
                continue;
            }

            if (ImGui::MenuItem(lDrawer->GetComponentName()))
            {
                lDrawer->Add(InWorld, InEntity);
                ImGui::CloseCurrentPopup();
            }
        }
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
