#pragma once

#include <nlohmann/json.hpp>

#include "Core/Tag/OpaaxTag.h"
#include "Core/Tag/OpaaxTagContainer.h"

// =============================================================================
// JSON for tags: a tag is a string, a container is an array of strings.
// =============================================================================
namespace Opaax
{
    inline void to_json(nlohmann::json& Json, const OpaaxTag& Tag)
    {
        // GetView, not ToString: an invalid tag writes "" (not "None").
        const OpaaxStringView lText = Tag.GetView();
        Json = lText.IsEmpty() ? std::string() : std::string(lText.Data(), lText.GetLength());
    }

    inline void from_json(const nlohmann::json& Json, OpaaxTag& Tag)
    {
        const std::string     lText = Json.get<std::string>();
        const OpaaxStringView lView(lText);

        // Malformed text reads as the invalid tag (no assert).
        Tag = OpaaxTag::IsValidTagText(lView) ? OpaaxTag(lView) : OpaaxTag();
    }

    inline void to_json(nlohmann::json& Json, const OpaaxTagContainer& Container)
    {
        Json = nlohmann::json::array();
        for (const OpaaxTag lTag : Container) { Json.emplace_back(lTag); }
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
