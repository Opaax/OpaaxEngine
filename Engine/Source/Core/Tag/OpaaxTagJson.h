#pragma once

#include <nlohmann/json.hpp>

#include "Core/Tag/OpaaxTag.h"
#include "Core/Tag/OpaaxTagContainer.h"

// =============================================================================
// The nlohmann bridge for tags — a tag is a STRING and a container is an ARRAY of them, because
// that is what a human editing a .opaaxmap should see.
//
// Split out of the tag headers themselves, the way Core/Maths/MathsJson.hpp is split from
// MathTypes.h: code that only matches tags never pays for json. Include this one to put an
// OpaaxTag or an OpaaxTagContainer inside a component's NLOHMANN_DEFINE_TYPE_INTRUSIVE.
// =============================================================================
namespace Opaax
{
    inline void to_json(nlohmann::json& Json, const OpaaxTag& Tag)
    {
        // GetView, not ToString: an invalid tag must write "" and read back invalid, where ToString's
        // "None" would read back as a real tag literally named None.
        const OpaaxStringView lText = Tag.GetView();
        Json = lText.IsEmpty() ? std::string() : std::string(lText.Data(), lText.GetLength());
    }

    inline void from_json(const nlohmann::json& Json, OpaaxTag& Tag)
    {
        const std::string     lText = Json.get<std::string>();
        const OpaaxStringView lView(lText);

        // A hand-edited file is UNTRUSTED input, so malformed text reads back as the invalid tag
        // rather than tripping the ctor's assert — that net exists for literals in code.
        Tag = OpaaxTag::IsValidTagText(lView) ? OpaaxTag(lView) : OpaaxTag();
    }

    inline void to_json(nlohmann::json& Json, const OpaaxTagContainer& Container)
    {
        Json = nlohmann::json::array();
        for (const OpaaxTag lTag : Container) { Json.push_back(lTag); }
    }

    inline void from_json(const nlohmann::json& Json, OpaaxTagContainer& Container)
    {
        Container.Clear();
        if (!Json.is_array()) { return; }

        for (const nlohmann::json& lEntry : Json)
        {
            Container.AddTag(lEntry.get<OpaaxTag>());
        }
    }
} // namespace Opaax
