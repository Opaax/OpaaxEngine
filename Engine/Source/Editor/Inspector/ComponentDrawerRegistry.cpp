#include "ComponentDrawerRegistry.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

namespace Opaax::Editor
{
    namespace
    {
        TDynArray<UniquePtr<IComponentDrawer>>& GetRegistry()
        {
            static TDynArray<UniquePtr<IComponentDrawer>> s_Drawers;
            return s_Drawers;
        }
    }

    void ComponentDrawerRegistry::Register(UniquePtr<IComponentDrawer> InDrawer)
    {
        GetRegistry().push_back(Move(InDrawer));
    }

    void ComponentDrawerRegistry::DrawAll(World& InWorld, EntityID InEntity)
    {
        for (auto& lDrawer : GetRegistry())
        {
            if (lDrawer->HasComponent(InWorld, InEntity))
            {
                lDrawer->Draw(InWorld, InEntity);
            }
        }
    }

    void ComponentDrawerRegistry::DrawAddComponentMenu(World& InWorld, EntityID InEntity)
    {
        for (auto& lDrawer : GetRegistry())
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

    void ComponentDrawerRegistry::Clear()
    {
        GetRegistry().clear();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
