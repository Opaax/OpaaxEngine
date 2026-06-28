#pragma once

#include "EngineConfigData.h"
#include "TConfig.hpp"
#include "Core/EngineAPI.h"


namespace Opaax
{
    // =============================================================================
    // Config_Engine — the engine runtime config block (window/render/physics/...),
    // loaded from <ProjectRoot>/Configs/<FileName>. All Load/Save plumbing lives in
    // TConfig<EngineConfigData>; this type only supplies its identity (type tag +
    // file name) and its data type.
    // =============================================================================
    class OPAAX_API Config_Engine final : public TConfig<EngineConfigData>
    {
    public:
        OPAAX_CONFIG_TYPE(Engine)

        const char* FileName() const override { return "Engine.config"; }
    };
}
