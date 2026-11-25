#pragma once

#include <nlohmann/json.hpp>

#include "Application/Services/ICommandRegistry.h"
#include "Commands/CommandHandle.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringJson.h"
#include "Renderer/Textures/RuntimeTexture.h"
#include "Renderer/Textures/Texture2D.h"
#include "Renderer/Textures/TexturePaint.h"
#include "RHI/Framebuffer.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/World.h"
#include "World/WorldSpec.h"

namespace TestWorld
{
    // =============================================================================
    // PaintTextureProbeSubsystem — a world subsystem of the PaintTexture level's Play worlds: makes
    //   paintable textures and paints, clears and releases them, as a game would, from a script.
    //   probe.paint.make {path, width, height, checker}: transparent, or a one-texel black and white checker.
    //   probe.paint.stamps {path, mode, stamps: [{texture, sheet, frame, color, center, size, rotation}]}.
    //   probe.paint.clear {path, color, x, y, width, height}: all of it without width and height.
    //   probe.paint.texel {path, x, y}: the texel painted there (read back from the GPU: tests only).
    //   probe.paint.release {path}: lets go of it (released at the next resource update).
    //   The textures still held go with the world.
    // =============================================================================
    class PaintTextureProbeSubsystem final : public Opaax::WorldSubsystem
    {
        OPAAX_OBJECT(PaintTextureProbeSubsystem, Opaax::WorldSubsystem)

        bool ShouldCreate(const Opaax::World& InWorld) const override
        {
            return InWorld.GetMode() == Opaax::EWorldMode::Play
                && InWorld.GetName() == Opaax::OpaaxStringID("PaintTexture");
        }

        bool Startup() override
        {
            if (GetContext().Commands == nullptr)
            {
                return true;
            }

            m_Make = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.paint.make", "TestWorld: makes a paintable texture under a runtime path: "
                                                       "{path, width, height, paintable}.")
                    .Param("path", Opaax::ECommandParamType::String, "a runtime path, \"/Runtime/<Name>\"")
                    .Param("width", Opaax::ECommandParamType::Int, "texels across")
                    .Param("height", Opaax::ECommandParamType::Int, "texels up")
                    .Param("checker", Opaax::ECommandParamType::Bool,
                           "start as a one-texel black and white checker, not transparent (false)").Optional(),
                [this](const nlohmann::json& InParams) { return Make(InParams); });

            m_Stamps = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.paint.stamps", "TestWorld: paints stamps into a made texture, drawn before "
                                                         "the next frame's views: {queued}.")
                    .Param("path", Opaax::ECommandParamType::String, "the texture's runtime path")
                    .Param("mode", Opaax::ECommandParamType::String, "Over (painted over) or Erase (Over)")
                    .Choices({ "Over", "Erase" }).Optional()
                    .Param("stamps", Opaax::ECommandParamType::Any,
                           "[{texture, sheet, frame, color [r, g, b, a] 0..1, center [x, y], size [w, h], rotation}], "
                           "in texels from the bottom-left corner, rotation in radians"),
                [this](const nlohmann::json& InParams) { return Stamps(InParams); });

            m_Clear = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.paint.clear", "TestWorld: clears a made texture, or a rectangle of it, to "
                                                        "a colour: {cleared}.")
                    .Param("path", Opaax::ECommandParamType::String, "the texture's runtime path")
                    .Param("color", Opaax::ECommandParamType::Any, "[r, g, b, a] 0..1 (transparent)").Optional()
                    .Param("x", Opaax::ECommandParamType::Int, "first texel across (0)").Optional()
                    .Param("y", Opaax::ECommandParamType::Int, "first texel up (0)").Optional()
                    .Param("width", Opaax::ECommandParamType::Int, "texels across (all of it)").Optional()
                    .Param("height", Opaax::ECommandParamType::Int, "texels up (all of it)").Optional(),
                [this](const nlohmann::json& InParams) { return Clear(InParams); });

            m_Texel = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.paint.texel", "TestWorld: reads one texel of a painted texture back from "
                                                        "the GPU, premultiplied: {texel}.")
                    .Param("path", Opaax::ECommandParamType::String, "the texture's runtime path")
                    .Param("x", Opaax::ECommandParamType::Int, "texel across")
                    .Param("y", Opaax::ECommandParamType::Int, "texel up, from the bottom row"),
                [this](const nlohmann::json& InParams) { return Texel(InParams); });

            m_Release = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.paint.release", "TestWorld: lets go of a made texture: {path, held}.")
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
        /** The texture made under the path of InParams, or null. */
        Opaax::RuntimeTexture* Find(const nlohmann::json& InParams)
        {
            const Opaax::OpaaxString lPath = InParams.value("path", Opaax::OpaaxString());
            const auto lIt = m_Textures.find(Opaax::OpaaxStringID(lPath.CStr()).GetId());
            return lIt != m_Textures.end() ? &lIt->second : nullptr;
        }

        /** A vector of InCount floats in InParams[InKey], or InDefault when it is not one. */
        template<typename TVector>
        static TVector Floats(const nlohmann::json& InParams, const char* InKey, const TVector& InDefault)
        {
            const auto lIt = InParams.find(InKey);
            if (lIt == InParams.end() || !lIt->is_array() || lIt->size() != static_cast<size_t>(TVector::length()))
            {
                return InDefault;
            }

            TVector lValue = InDefault;
            for (int i = 0; i < TVector::length(); ++i)
            {
                lValue[i] = (*lIt)[static_cast<size_t>(i)].template get<float>();
            }
            return lValue;
        }

        Opaax::CommandResult Make(const nlohmann::json& InParams)
        {
            const Opaax::OpaaxString lPath = InParams.value("path", Opaax::OpaaxString());

            Opaax::RuntimeTextureDesc lDesc;
            lDesc.Width      = static_cast<Opaax::Uint32>(InParams.value("width", 0));
            lDesc.Height     = static_cast<Opaax::Uint32>(InParams.value("height", 0));
            lDesc.bPaintable = true;

            // Straight RGBA, bottom row first: black where x + y is even, white where it is odd.
            Opaax::TDynArray<Opaax::Uint8> lChecker;
            if (InParams.value("checker", false))
            {
                lChecker.reserve(static_cast<size_t>(lDesc.Width) * lDesc.Height * 4u);
                for (Opaax::Uint32 lY = 0; lY < lDesc.Height; ++lY)
                {
                    for (Opaax::Uint32 lX = 0; lX < lDesc.Width; ++lX)
                    {
                        const Opaax::Uint8 lValue = ((lX + lY) % 2u == 0u) ? 0u : 255u;
                        lChecker.insert(lChecker.end(), { lValue, lValue, lValue, 255u });
                    }
                }
            }

            Opaax::RuntimeTexture lTexture = Opaax::RuntimeTexture::Create(
                GetContext().Resources, lPath, lDesc, lChecker.empty() ? nullptr : lChecker.data());
            if (!lTexture.IsValid())
            {
                return Opaax::CommandResult::Fail("the texture was not made: the log says why");
            }

            m_Textures[Opaax::OpaaxStringID(lPath.CStr()).GetId()] = lTexture;
            return Opaax::CommandResult::Ok(nlohmann::json{ { "path", Opaax::ToJsonString(lTexture.GetPath()) },
                                                            { "width", lTexture.GetWidth() },
                                                            { "height", lTexture.GetHeight() },
                                                            { "paintable", lTexture.IsPaintable() } });
        }

        Opaax::CommandResult Stamps(const nlohmann::json& InParams)
        {
            Opaax::RuntimeTexture* lTexture = Find(InParams);
            if (lTexture == nullptr)
            {
                return Opaax::CommandResult::Fail("no texture made under this path: probe.paint.make first");
            }

            const auto lList = InParams.find("stamps");
            if (lList == InParams.end() || !lList->is_array())
            {
                return Opaax::CommandResult::Fail("stamps must be a list");
            }

            Opaax::TDynArray<Opaax::PaintStamp> lStamps;
            for (const nlohmann::json& lEntry : *lList)
            {
                Opaax::PaintStamp lStamp;
                lStamp.Texture.Path = lEntry.value("texture", Opaax::OpaaxString());
                lStamp.Sheet.Path   = lEntry.value("sheet", Opaax::OpaaxString());
                lStamp.Frame        = lEntry.value("frame", 0);
                lStamp.Color        = Floats(lEntry, "color", lStamp.Color);
                lStamp.Center       = Floats(lEntry, "center", lStamp.Center);
                lStamp.Size         = Floats(lEntry, "size", lStamp.Size);
                lStamp.Rotation     = lEntry.value("rotation", 0.f);
                lStamps.push_back(lStamp);
            }

            const Opaax::EPaintMode lMode = InParams.value("mode", Opaax::OpaaxString("Over")) == Opaax::OpaaxString("Erase")
                                          ? Opaax::EPaintMode::Erase
                                          : Opaax::EPaintMode::Over;
            if (!lTexture->Paint(lStamps, lMode))
            {
                return Opaax::CommandResult::Fail("not painted: the log says why");
            }

            return Opaax::CommandResult::Ok(nlohmann::json{ { "queued", lStamps.size() } });
        }

        Opaax::CommandResult Clear(const nlohmann::json& InParams)
        {
            Opaax::RuntimeTexture* lTexture = Find(InParams);
            if (lTexture == nullptr)
            {
                return Opaax::CommandResult::Fail("no texture made under this path: probe.paint.make first");
            }

            const Opaax::Vector4F lColor = Floats(InParams, "color", Opaax::Vector4F{ 0.f, 0.f, 0.f, 0.f });

            bool bCleared = false;
            if (InParams.contains("width") || InParams.contains("height"))
            {
                Opaax::TextureRegion lRegion;
                lRegion.X      = static_cast<Opaax::Uint32>(InParams.value("x", 0));
                lRegion.Y      = static_cast<Opaax::Uint32>(InParams.value("y", 0));
                lRegion.Width  = static_cast<Opaax::Uint32>(InParams.value("width", 0));
                lRegion.Height = static_cast<Opaax::Uint32>(InParams.value("height", 0));
                bCleared       = lTexture->ClearRegion(lRegion, lColor);
            }
            else
            {
                bCleared = lTexture->Clear(lColor);
            }

            if (!bCleared)
            {
                return Opaax::CommandResult::Fail("not cleared: the log says why");
            }
            return Opaax::CommandResult::Ok(nlohmann::json{ { "cleared", true } });
        }

        Opaax::CommandResult Texel(const nlohmann::json& InParams)
        {
            Opaax::RuntimeTexture* lTexture = Find(InParams);
            if (lTexture == nullptr || lTexture->GetTexture() == nullptr
                || lTexture->GetTexture()->GetPaintTarget() == nullptr)
            {
                return Opaax::CommandResult::Fail("no painted texture under this path (or no device)");
            }

            const Opaax::Uint32 lX = static_cast<Opaax::Uint32>(InParams.value("x", 0));
            const Opaax::Uint32 lY = static_cast<Opaax::Uint32>(InParams.value("y", 0));
            const Opaax::Uint32 lWidth  = lTexture->GetWidth();
            const Opaax::Uint32 lHeight = lTexture->GetHeight();
            if (lX >= lWidth || lY >= lHeight)
            {
                return Opaax::CommandResult::Fail("the texel is outside the texture");
            }

            Opaax::TDynArray<Opaax::Uint8> lRGBA;
            if (!lTexture->GetTexture()->GetPaintTarget()->ReadPixels(lRGBA))
            {
                return Opaax::CommandResult::Fail("the texture could not be read back");
            }

            // Read back top row first.
            const size_t       lAt     = (static_cast<size_t>(lHeight - 1u - lY) * lWidth + lX) * 4u;
            const nlohmann::json lTexel = nlohmann::json::array({ lRGBA[lAt], lRGBA[lAt + 1], lRGBA[lAt + 2], lRGBA[lAt + 3] });
            return Opaax::CommandResult::Ok(nlohmann::json{ { "texel", lTexel } });
        }

        Opaax::CommandHandle m_Make;
        Opaax::CommandHandle m_Stamps;
        Opaax::CommandHandle m_Clear;
        Opaax::CommandHandle m_Texel;
        Opaax::CommandHandle m_Release;

        Opaax::TUnorderedMap<Opaax::Uint32, Opaax::RuntimeTexture> m_Textures;   // by path id
    };
}
