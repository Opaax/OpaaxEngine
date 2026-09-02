#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    // =============================================================================
    // The nlohmann bridge for OpaaxStringID, SPLIT from the type the way OpaaxStringJson.h and
    // OpaaxTagJson.h are: matching ids must not drag a json library behind them.
    //
    // IT WRITES THE TEXT, NEVER THE ID. An OpaaxStringID is an intern-table INDEX and the table is
    // built in the order a process happens to intern things — so a number written today names a
    // different string tomorrow. MapJson has hand-converted for exactly this reason since M5; this
    // is that rule as one bridge, so a type holding an id gets it from the macro like any field.
    // =============================================================================
    inline void to_json(nlohmann::json& InJson, const OpaaxStringID& InValue)
    {
        // IsValid gates it: the invalid id resolves to the pool's "None", which would read back as a
        // real name spelled None (the trap I14 hit). Empty text is the honest spelling of "unnamed".
        InJson = InValue.IsValid() ? std::string(InValue.CStr()) : std::string();
    }

    inline void from_json(const nlohmann::json& InJson, OpaaxStringID& InValue)
    {
        // Throws type_error on a non-string, which the callers above already catch — tolerance lives
        // at the file boundary, once, rather than in every field.
        const std::string lText = InJson.get<std::string>();

        InValue = lText.empty() ? OpaaxStringID() : OpaaxStringID(lText);
    }
}
