#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax::Editor
{
    /** What a copy holds. Paste checks it, so entities never land in a canvas. */
    enum class EClipboardKind : Uint8
    {
        Entities,     // MapJson text of the copied entities
        Component,    // one component: its type name and values
        Properties,   // an object's property values (an asset's)
        UIWidget      // a UI widget node (UICanvasFile::SerializeNode)
    };

    // =============================================================================
    // ClipboardPayload — the editor's clipboard text: readable JSON in an envelope that says what it
    //   holds, {"opaaxClipboard": "Entities", "version": 1, "data": ...}. Pasted into a text editor,
    //   it reads as it is. Header-only, so the tests use it without the editor.
    // =============================================================================
    namespace ClipboardPayload
    {
        inline constexpr const char* KEY_KIND    = "opaaxClipboard";
        inline constexpr const char* KEY_VERSION = "version";
        inline constexpr const char* KEY_DATA    = "data";

        /** The format this build writes, and the highest it reads. */
        inline constexpr Uint32 VERSION = 1;

        inline const char* ToString(const EClipboardKind InKind) noexcept
        {
            switch (InKind)
            {
                case EClipboardKind::Entities:   return "Entities";
                case EClipboardKind::Component:  return "Component";
                case EClipboardKind::Properties: return "Properties";
                case EClipboardKind::UIWidget:   return "UIWidget";
            }
            return "Entities";
        }

        inline bool FromString(const OpaaxString& InText, EClipboardKind& OutKind) noexcept
        {
            for (const EClipboardKind lKind : { EClipboardKind::Entities, EClipboardKind::Component,
                                                EClipboardKind::Properties, EClipboardKind::UIWidget })
            {
                if (InText == OpaaxString(ToString(lKind)))
                {
                    OutKind = lKind;
                    return true;
                }
            }
            return false;
        }

        /** InData in the envelope, indented. */
        inline OpaaxString Make(const EClipboardKind InKind, const nlohmann::json& InData)
        {
            nlohmann::json lEnvelope = nlohmann::json::object();
            lEnvelope[KEY_KIND]    = ToString(InKind);
            lEnvelope[KEY_VERSION] = VERSION;
            lEnvelope[KEY_DATA]    = InData;

            return ToJsonText(lEnvelope, 4);
        }

        /**
         * The data of a copy of InExpected. Never throws.
         * @param OutError Why not: not the editor's clipboard text, a newer version, or another kind (both named)
         * @return False when InText is not a copy of InExpected
         */
        inline bool Parse(const OpaaxString& InText, const EClipboardKind InExpected, nlohmann::json& OutData,
                          OpaaxString& OutError)
        {
            const nlohmann::json lJson = nlohmann::json::parse(ToJsonString(InText), nullptr, false);

            const auto lKind = lJson.is_object() ? lJson.find(KEY_KIND) : lJson.end();
            if (!lJson.is_object() || lKind == lJson.end() || !lKind->is_string() || !lJson.contains(KEY_DATA))
            {
                OutError = "the clipboard holds no copy from the editor";
                return false;
            }

            const Uint32 lVersion = lJson.value(KEY_VERSION, 0u);
            if (lVersion > VERSION)
            {
                OutError = OpaaxString::Format("the clipboard holds a copy of version {}, newer than this editor's {}",
                                               lVersion, VERSION);
                return false;
            }

            const OpaaxString lKindText = FromJsonString(lKind->get_ref<const nlohmann::json::string_t&>());
            EClipboardKind    lFound    = EClipboardKind::Entities;
            if (!FromString(lKindText, lFound) || lFound != InExpected)
            {
                OutError = OpaaxString::Format("the clipboard holds {}, not {}", lKindText, ToString(InExpected));
                return false;
            }

            OutData = lJson.at(KEY_DATA);
            return true;
        }
    }
}
