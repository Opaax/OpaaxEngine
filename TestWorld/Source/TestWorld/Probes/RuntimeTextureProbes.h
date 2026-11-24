#pragma once

#include <nlohmann/json.hpp>

#include "Application/Services/ICommandRegistry.h"
#include "Commands/CommandHandle.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringJson.h"
#include "Renderer/Textures/RuntimeTexture.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/World.h"
#include "World/WorldSpec.h"

namespace TestWorld
{
    // =============================================================================
    // RuntimeTextureProbeSubsystem — a world subsystem of the RuntimeTexture level's Play worlds: makes,
    //   fills and releases textures made from pixels, as a game would, from a script.
    //   probe.texture.make {path, width, height, channels, texel, smooth}: every texel the bytes of texel.
    //   probe.texture.fill {path, x, y, width, height, texel}: a rectangle of texels set to texel.
    //   probe.texture.release {path}: lets go of it (released at the next resource update).
    //   The textures still held go with the world.
    // =============================================================================
    class RuntimeTextureProbeSubsystem final : public Opaax::WorldSubsystem
    {
        OPAAX_OBJECT(RuntimeTextureProbeSubsystem, Opaax::WorldSubsystem)

        bool ShouldCreate(const Opaax::World& InWorld) const override
        {
            return InWorld.GetMode() == Opaax::EWorldMode::Play
                && InWorld.GetName() == Opaax::OpaaxStringID("RuntimeTexture");
        }

        bool Startup() override
        {
            if (GetContext().Commands == nullptr)
            {
                return true;
            }

            m_Make = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.texture.make", "TestWorld: makes a texture from pixels under a runtime path, "
                                                         "every texel the bytes of texel: {path, width, height, channels}.")
                    .Param("path", Opaax::ECommandParamType::String, "a runtime path, \"/Runtime/<Name>\"")
                    .Param("width", Opaax::ECommandParamType::Int, "texels across")
                    .Param("height", Opaax::ECommandParamType::Int, "texels up")
                    .Param("channels", Opaax::ECommandParamType::Int, "1 or 4")
                    .Param("texel", Opaax::ECommandParamType::Any, "one byte per channel, [r, g, b, a] or [value]")
                    .Param("smooth", Opaax::ECommandParamType::Bool, "read blended whatever the filter (false)").Optional(),
                [this](const nlohmann::json& InParams) { return Make(InParams); });

            m_Fill = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.texture.fill", "TestWorld: sets a rectangle of a made texture's texels, "
                                                         "x, y from its bottom-left texel: {path, x, y, width, height}.")
                    .Param("path", Opaax::ECommandParamType::String, "the texture's runtime path")
                    .Param("x", Opaax::ECommandParamType::Int, "first texel across")
                    .Param("y", Opaax::ECommandParamType::Int, "first texel up")
                    .Param("width", Opaax::ECommandParamType::Int, "texels across")
                    .Param("height", Opaax::ECommandParamType::Int, "texels up")
                    .Param("texel", Opaax::ECommandParamType::Any, "one byte per channel"),
                [this](const nlohmann::json& InParams) { return Fill(InParams); });

            m_Release = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.texture.release", "TestWorld: lets go of a made texture: {path, held}.")
                    .Param("path", Opaax::ECommandParamType::String, "the texture's runtime path"),
                [this](const nlohmann::json& InParams)
                {
                    const Opaax::OpaaxString lPath = InParams.value("path", Opaax::OpaaxString());
                    const bool               lHeld = m_Textures.erase(Opaax::OpaaxStringID(lPath.CStr()).GetId()) > 0;
                    return Opaax::CommandResult::Ok(nlohmann::json{ { "path", Opaax::ToJsonString(lPath) },
                                                                    { "held", lHeld } });
                });
            return true;
        }

        void Shutdown() override { m_Textures.clear(); }

    private:
        /** InCount texels of the bytes in InTexel, or empty when InTexel does not have InChannels bytes. */
        static Opaax::TDynArray<Opaax::Uint8> Texels(const nlohmann::json& InTexel, const Opaax::Int32 InChannels,
                                                     const Opaax::Uint64 InCount)
        {
            Opaax::TDynArray<Opaax::Uint8> lPixels;
            if (!InTexel.is_array() || InTexel.size() != static_cast<size_t>(InChannels))
            {
                return lPixels;
            }

            lPixels.reserve(static_cast<size_t>(InCount) * InChannels);
            for (Opaax::Uint64 lTexel = 0; lTexel < InCount; ++lTexel)
            {
                for (const nlohmann::json& lByte : InTexel)
                {
                    lPixels.push_back(static_cast<Opaax::Uint8>(lByte.get<int>()));
                }
            }
            return lPixels;
        }

        Opaax::CommandResult Make(const nlohmann::json& InParams)
        {
            const Opaax::OpaaxString lPath = InParams.value("path", Opaax::OpaaxString());

            Opaax::RuntimeTextureDesc lDesc;
            lDesc.Width    = static_cast<Opaax::Uint32>(InParams.value("width", 0));
            lDesc.Height   = static_cast<Opaax::Uint32>(InParams.value("height", 0));
            lDesc.Channels = InParams.value("channels", 4);
            lDesc.bSmooth  = InParams.value("smooth", false);

            const Opaax::TDynArray<Opaax::Uint8> lPixels =
                Texels(InParams.value("texel", nlohmann::json()), lDesc.Channels,
                       static_cast<Opaax::Uint64>(lDesc.Width) * lDesc.Height);
            if (lPixels.empty())
            {
                return Opaax::CommandResult::Fail("texel must hold one byte per channel");
            }

            Opaax::RuntimeTexture lTexture =
                Opaax::RuntimeTexture::Create(GetContext().Resources, lPath, lDesc, lPixels.data());
            if (!lTexture.IsValid())
            {
                return Opaax::CommandResult::Fail("the texture was not made: the log says why");
            }

            m_Textures[Opaax::OpaaxStringID(lPath.CStr()).GetId()] = lTexture;
            return Opaax::CommandResult::Ok(nlohmann::json{ { "path", Opaax::ToJsonString(lTexture.GetPath()) },
                                                            { "width", lTexture.GetWidth() },
                                                            { "height", lTexture.GetHeight() },
                                                            { "channels", lTexture.GetChannels() } });
        }

        Opaax::CommandResult Fill(const nlohmann::json& InParams)
        {
            const Opaax::OpaaxString lPath = InParams.value("path", Opaax::OpaaxString());

            const auto lIt = m_Textures.find(Opaax::OpaaxStringID(lPath.CStr()).GetId());
            if (lIt == m_Textures.end())
            {
                return Opaax::CommandResult::Fail("no texture made under this path: probe.texture.make first");
            }

            Opaax::TextureRegion lRegion;
            lRegion.X      = static_cast<Opaax::Uint32>(InParams.value("x", 0));
            lRegion.Y      = static_cast<Opaax::Uint32>(InParams.value("y", 0));
            lRegion.Width  = static_cast<Opaax::Uint32>(InParams.value("width", 0));
            lRegion.Height = static_cast<Opaax::Uint32>(InParams.value("height", 0));

            const Opaax::TDynArray<Opaax::Uint8> lPixels =
                Texels(InParams.value("texel", nlohmann::json()), lIt->second.GetChannels(),
                       static_cast<Opaax::Uint64>(lRegion.Width) * lRegion.Height);
            if (lPixels.empty() || !lIt->second.UpdateRegion(lRegion, lPixels.data()))
            {
                return Opaax::CommandResult::Fail("not filled: texel must hold one byte per channel and the "
                                                  "rectangle lie inside the texture");
            }

            return Opaax::CommandResult::Ok(nlohmann::json{ { "filled", true } });
        }

        Opaax::CommandHandle m_Make;
        Opaax::CommandHandle m_Fill;
        Opaax::CommandHandle m_Release;

        Opaax::TUnorderedMap<Opaax::Uint32, Opaax::RuntimeTexture> m_Textures;   // by path id
    };
}
