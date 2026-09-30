#include "Editor/Resources/ResourceDragDrop.h"

#include <cstring>   // memcpy

#include <imgui.h>

#include "Core/Log/Logger.h"

namespace Opaax::Editor
{
    namespace
    {
        // A drop happens once per mouse release, so logging it does not flood, and it tells "the field
        // refused this type" apart from "the drag never worked".
        constexpr LogCategory LogResourceDragDrop{"ResourceDragDrop"};

        /** The type id a payload carries, or 0 when it is not ours or is truncated. */
        Uint32 PayloadTypeId(const ImGuiPayload* InPayload)
        {
            if (InPayload == nullptr || !InPayload->IsDataType(RESOURCE_PAYLOAD_ID))
            {
                return 0;
            }

            if (InPayload->DataSize < static_cast<int>(sizeof(Uint32)))
            {
                return 0;
            }

            Uint32 lTypeId = 0;
            std::memcpy(&lTypeId, InPayload->Data, sizeof(Uint32));

            return lTypeId;
        }
    }

    void SetResourceDragPayload(const Uint32 InTypeId, const OpaaxString& InAssetPath)
    {
        if (InAssetPath.IsEmpty())
        {
            return;
        }

        // Built every frame of the drag: ImGui copies the bytes immediately.
        TDynArray<Uint8> lBytes(sizeof(Uint32) + InAssetPath.GetLength());
        std::memcpy(lBytes.data(), &InTypeId, sizeof(Uint32));
        std::memcpy(lBytes.data() + sizeof(Uint32), InAssetPath.CStr(), InAssetPath.GetLength());

        ImGui::SetDragDropPayload(RESOURCE_PAYLOAD_ID, lBytes.data(), lBytes.size());
    }

    bool AcceptResourceDragPayload(const Uint32 InTypeId, OpaaxString& OutAssetPath)
    {
        if (!ImGui::BeginDragDropTarget())
        {
            return false;
        }

        bool lAccepted = false;

        // Peek before accepting: all resources share one payload id, so accepting first would highlight
        // this widget for a type it then refuses.
        if (PayloadTypeId(ImGui::GetDragDropPayload()) == InTypeId)
        {
            if (const ImGuiPayload* lPayload = ImGui::AcceptDragDropPayload(RESOURCE_PAYLOAD_ID))
            {
                const char*  lText   = static_cast<const char*>(lPayload->Data) + sizeof(Uint32);
                const Uint32 lLength = static_cast<Uint32>(lPayload->DataSize) - sizeof(Uint32);

                OutAssetPath = OpaaxString(lText, lLength);   // the payload is not null-terminated
                lAccepted    = true;

                OPAAX_LOG(LogResourceDragDrop, Info, "Accepted '{}' (resource type {})",
                          OutAssetPath.CStr(), InTypeId);
            }
        }

        ImGui::EndDragDropTarget();

        return lAccepted;
    }
}
