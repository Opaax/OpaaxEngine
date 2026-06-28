#include "EngineConfigData.h"

#include <nlohmann/json.hpp>

namespace Opaax
{
    namespace KEcfg = Opaax_Engine_Config;

    // =========================================================================
    // Pure parse — start from defaults, override each field defensively.
    // =========================================================================
    EngineConfigData ParseEngineConfig(const OpaaxString& InJsonText)
    {
        EngineConfigData lData; // defaults
        if (InJsonText.IsEmpty())
        {
            return lData;
        }

        nlohmann::json lRoot;
        try
        {
            lRoot = nlohmann::json::parse(InJsonText.CStr());
        }
        catch (const nlohmann::json::parse_error&)
        {
            return lData;
        }

        if (lRoot.contains(KEcfg::WINDOW_KEY) && lRoot[KEcfg::WINDOW_KEY].is_object())
        {
            const auto& lWin = lRoot[KEcfg::WINDOW_KEY];
            if (lWin.contains(KEcfg::TITLE_KEY) && lWin[KEcfg::TITLE_KEY].is_string())
            {
                lData.WindowTitle = OpaaxString(lWin[KEcfg::TITLE_KEY].get<std::string>().c_str());
            }
            if (lWin.contains(KEcfg::WIDTH_KEY) && lWin[KEcfg::WIDTH_KEY].is_number_unsigned())
            {
                lData.WindowWidth = lWin[KEcfg::WIDTH_KEY].get<Uint32>();
            }
            if (lWin.contains(KEcfg::HEIGHT_KEY) && lWin[KEcfg::HEIGHT_KEY].is_number_unsigned())
            {
                lData.WindowHeight = lWin[KEcfg::HEIGHT_KEY].get<Uint32>();
            }
        }

        if (lRoot.contains(KEcfg::ASSETS_KEY) && lRoot[KEcfg::ASSETS_KEY].is_object())
        {
            const auto& lA = lRoot[KEcfg::ASSETS_KEY];
            if (lA.contains(KEcfg::ENGINE_ROOT_KEY) && lA[KEcfg::ENGINE_ROOT_KEY].is_string())
            {
                lData.EngineAssetsRoot = OpaaxString(lA[KEcfg::ENGINE_ROOT_KEY].get<std::string>().c_str());
            }
            
            if (lA.contains(KEcfg::ENGINE_MANIFEST_KEY) && lA[KEcfg::ENGINE_MANIFEST_KEY].is_string())
            {
                lData.EngineManifestRelPath = OpaaxString(lA[KEcfg::ENGINE_MANIFEST_KEY].get<std::string>().c_str());
            }
        }

        if (lRoot.contains(KEcfg::LOG_KEY) && lRoot[KEcfg::LOG_KEY].is_object())
        {
            const auto& lLog = lRoot[KEcfg::LOG_KEY];
            if (lLog.contains(KEcfg::LEVEL_KEY) && lLog[KEcfg::LEVEL_KEY].is_string())
            {
                lData.LogLevel = OpaaxString(lLog[KEcfg::LEVEL_KEY].get<std::string>().c_str());
            }
        }

        if (lRoot.contains(KEcfg::RENDER_KEY) && lRoot[KEcfg::RENDER_KEY].is_object())
        {
            const auto& lR = lRoot[KEcfg::RENDER_KEY];
            if (lR.contains(KEcfg::BACKEND_KEY) && lR[KEcfg::BACKEND_KEY].is_string())
            {
                lData.RenderBackend = OpaaxString(lR[KEcfg::BACKEND_KEY].get<std::string>().c_str());
            }
            
            if (lR.contains(KEcfg::INTERPOLATION_KEY) && lR[KEcfg::INTERPOLATION_KEY].is_boolean())
            {
                lData.RenderInterpolation = lR[KEcfg::INTERPOLATION_KEY].get<bool>();
            }
        }

        if (lRoot.contains(KEcfg::PHYSICS_KEY) && lRoot[KEcfg::PHYSICS_KEY].is_object())
        {
            const auto& lP = lRoot[KEcfg::PHYSICS_KEY];
            if (lP.contains(KEcfg::BACKEND_KEY) && lP[KEcfg::BACKEND_KEY].is_string())
            {
                lData.PhysicsBackend = OpaaxString(lP[KEcfg::BACKEND_KEY].get<std::string>().c_str());
            }

            if (lP.contains(KEcfg::WORLD_BOUNDS_KEY) && lP[KEcfg::WORLD_BOUNDS_KEY].is_object())
            {
                const auto& lWB = lP[KEcfg::WORLD_BOUNDS_KEY];
                
                if (lWB.contains(KEcfg::ENABLED_KEY) && lWB[KEcfg::ENABLED_KEY].is_boolean())
                {
                    lData.PhysicsWorldBoundsEnabled = lWB[KEcfg::ENABLED_KEY].get<bool>();
                }
                
                if (lWB.contains(KEcfg::MIN_KEY) && lWB[KEcfg::MIN_KEY].is_array() && lWB[KEcfg::MIN_KEY].size() == 2
                    && lWB[KEcfg::MIN_KEY][0].is_number() && lWB[KEcfg::MIN_KEY][1].is_number())
                {
                    lData.PhysicsWorldBoundsMin = {
                        lWB[KEcfg::MIN_KEY][0].get<float>(), lWB[KEcfg::MIN_KEY][1].get<float>()
                    };
                }
                
                if (lWB.contains(KEcfg::MAX_KEY) && lWB[KEcfg::MAX_KEY].is_array() && lWB[KEcfg::MAX_KEY].size() == 2
                    && lWB[KEcfg::MAX_KEY][0].is_number() && lWB[KEcfg::MAX_KEY][1].is_number())
                {
                    lData.PhysicsWorldBoundsMax = {
                        lWB[KEcfg::MAX_KEY][0].get<float>(), lWB[KEcfg::MAX_KEY][1].get<float>()
                    };
                }
                    
                if (lWB.contains(KEcfg::RESPONSE_KEY) && lWB[KEcfg::RESPONSE_KEY].is_string())
                {
                    lData.PhysicsWorldBoundsResponse = OpaaxString(lWB[KEcfg::RESPONSE_KEY].get<std::string>().c_str());
                }
            }
        }

        return lData;
    }

    // =========================================================================
    // Pure serialize — same nested shape as EngineConfig's generated file.
    // =========================================================================
    OpaaxString SerializeEngineConfig(const EngineConfigData& InData)
    {
        nlohmann::json lRoot;
        lRoot[KEcfg::VERSION_KEY] = 1;
        lRoot[KEcfg::WINDOW_KEY]  = {
            { KEcfg::TITLE_KEY,  InData.WindowTitle.CStr() },
            { KEcfg::WIDTH_KEY,  InData.WindowWidth        },
            { KEcfg::HEIGHT_KEY, InData.WindowHeight       }
        };
        lRoot[KEcfg::ASSETS_KEY]  = {
            { KEcfg::ENGINE_ROOT_KEY,     InData.EngineAssetsRoot.CStr()      },
            { KEcfg::ENGINE_MANIFEST_KEY, InData.EngineManifestRelPath.CStr() }
        };
        lRoot[KEcfg::LOG_KEY]     = { { KEcfg::LEVEL_KEY, InData.LogLevel.CStr() } };
        lRoot[KEcfg::RENDER_KEY]  = {
            { KEcfg::BACKEND_KEY,       InData.RenderBackend.CStr() },
            { KEcfg::INTERPOLATION_KEY, InData.RenderInterpolation  }
        };
        lRoot[KEcfg::PHYSICS_KEY] = {
            { KEcfg::BACKEND_KEY, InData.PhysicsBackend.CStr() },
            { KEcfg::WORLD_BOUNDS_KEY, {
                { KEcfg::ENABLED_KEY,  InData.PhysicsWorldBoundsEnabled },
                { KEcfg::MIN_KEY,      { InData.PhysicsWorldBoundsMin.x, InData.PhysicsWorldBoundsMin.y } },
                { KEcfg::MAX_KEY,      { InData.PhysicsWorldBoundsMax.x, InData.PhysicsWorldBoundsMax.y } },
                { KEcfg::RESPONSE_KEY, InData.PhysicsWorldBoundsResponse.CStr() }
            }}
        };

        return OpaaxString(lRoot.dump(4).c_str());
    }
}
