#pragma once

#include <nlohmann/json.hpp>

#include "Core/GUID/Guid.h"

namespace Opaax
{
    // =============================================================================
    // The nlohmann bridge for Guid, SPLIT from the type the way LinearColorJson.h and
    // ResourcePathJson.h are — nothing that merely holds a Guid should drag json in.
    //
    // 32 LOWERCASE HEX, which is the ONE form a Guid takes on disk (**MP1**): the same text
    // MapJson already writes for an entity's identity, so a guid inside a component payload
    // and a guid in the entity header beside it read identically.
    // =============================================================================
    inline void to_json(nlohmann::json& InJson, const Guid& InValue)
    {
        InJson = InValue.ToString().CStr();
    }

    inline void from_json(const nlohmann::json& InJson, Guid& InValue)
    {
        // Throws type_error on a non-string, which MapFactory::Instantiate catches per
        // component (**I8**) — tolerance for the WRONG TYPE lives there, once.
        std::string lText;
        InJson.get_to(lText);

        // A well-typed but MALFORMED guid leaves the value INVALID rather than throwing. That is
        // MP3's rule for anything read at boot, and it costs nothing here because an invalid link
        // is a state the prefab layer must handle anyway — a prefab whose file was renamed
        // produces exactly the same thing, and the drawer says so.
        Guid lParsed;
        InValue = Guid::FromString(OpaaxString(lText.c_str(), static_cast<Uint32>(lText.size())), lParsed)
                      ? lParsed
                      : Guid{};
    }
}
