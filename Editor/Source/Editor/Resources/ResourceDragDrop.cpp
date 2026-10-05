#include "Editor/Resources/ResourceDragDrop.h"

#include <imgui.h>

#include "Core/Log/Logger.h"
#include "Editor/Resources/ResourceDragPayload.h"

namespace Opaax::Editor
{
    namespace
    {
        // A drop happens once per mouse release, so logging it does not flood, and it tells "the field
        // refused this type" apart from "the drag never worked".
        constexpr LogCategory LogResourceDragDrop{"ResourceDragDrop"};

        /** The payload being dragged, if it is ours. */
        bool DecodeCurrent(const ImGuiPayload* InPayload, ResourceDragPayload::Decoded& OutDecoded)
        {
            if (InPayload == nullptr || !InPayload->IsDataType(RESOURCE_PAYLOAD_ID))
            {
                return false;
            }

            return ResourceDragPayload::Decode(InPayload->Data, static_cast<Uint64>(InPayload->DataSize), OutDecoded);
        }
    }

    void SetResourceDragPayload(const Uint32 InTypeId, const OpaaxString& InAssetPath, const Uint32 InSubTypeId)
    {
        if (InAssetPath.IsEmpty())
        {
            return;
        }

        // Built every frame of the drag: ImGui copies the bytes immediately.
        const TDynArray<Uint8> lBytes = ResourceDragPayload::Encode(InTypeId, InSubTypeId, InAssetPath);
        ImGui::SetDragDropPayload(RESOURCE_PAYLOAD_ID, lBytes.data(), lBytes.size());
    }

    bool AcceptResourceDragPayload(const Uint32 InTypeId, OpaaxString& OutAssetPath, const Uint32 InSubTypeId)
    {
        if (!ImGui::BeginDragDropTarget())
        {
            return false;
        }

        bool lAccepted = false;

        // Peek before accepting: all resources share one payload id, so accepting first would highlight
        // this widget for a type it then refuses.
        ResourceDragPayload::Decoded lPeek;

        if (DecodeCurrent(ImGui::GetDragDropPayload(), lPeek) && ResourceDragPayload::Accepts(lPeek, InTypeId, InSubTypeId))
        {
            if (ImGui::AcceptDragDropPayload(RESOURCE_PAYLOAD_ID) != nullptr)
            {
                OutAssetPath = lPeek.AssetPath;
                lAccepted    = true;

                OPAAX_LOG(LogResourceDragDrop, Info, "Accepted '{}' (resource type {})",
                          OutAssetPath.CStr(), InTypeId);
            }
        }

        ImGui::EndDragDropTarget();

        return lAccepted;
    }
}
