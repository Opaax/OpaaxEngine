#include "TagDrawer.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "ECS/Components/TagComponent.h"

namespace Opaax::Editor
{
    bool TagDrawer::HasComponent(World& InWorld, EntityID InEntity) const
    {
        return InWorld.GetComponent<ECS::TagComponent>(InEntity) != nullptr;
    }

    void TagDrawer::Draw(World& InWorld, EntityID InEntity)
    {
        auto* lTag = InWorld.GetComponent<ECS::TagComponent>(InEntity);
        if (!lTag) { return; }

        if (ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen))
        {
            char lBuffer[256];
            strncpy_s(lBuffer, sizeof(lBuffer), lTag->Tag.ToString().CStr(), _TRUNCATE);

            if (ImGui::InputText("##Tag", lBuffer, sizeof(lBuffer)))
            {
                lTag->Tag = OpaaxStringID(lBuffer);
            }
        }
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
