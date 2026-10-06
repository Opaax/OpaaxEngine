#pragma once

#include "Core/EngineAPI.h"
#include "Core/Config/TConfig.hpp"

#include "Engine/Config/EngineConfigData.h"

namespace Opaax
{
    // =============================================================================
    // Config_Engine — the engine config (window, render, physics, ...),
    // loaded from <ProjectRoot>/Configs/Engine.config.
    // =============================================================================
    class Config_Engine final : public TConfig<EngineConfigData>
    {
    public:
        OPAAX_CONFIG_TYPE(Engine)

        const char* FileName() const override { return "Engine.config"; }
    };
}
