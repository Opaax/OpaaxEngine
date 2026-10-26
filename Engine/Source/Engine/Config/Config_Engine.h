#pragma once

#include "Engine/Config/EngineConfigData.h"
#include "Engine/Config/TConfig.h"

namespace Opaax
{
    // =============================================================================
    // Config_Engine — the engine config (window, render, physics, audio...), loaded from
    //   <ProjectRoot>/Configs/Engine.config.
    // =============================================================================
    class Config_Engine final : public TConfig<EngineConfigData>
    {
        OPAAX_OBJECT(Config_Engine, Config)

    public:
        const char* FileName() const override { return "Engine.config"; }
    };
}
