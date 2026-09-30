#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "UI/UIBinding.h"   // UIBindingHandle
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class World;
    class UIWidget;
    struct WorldContext;
    struct InputActionValue;
}

namespace Sandbox
{
    /** What the HUD shows: the model the authored tree reads as "Hud.<field>". */
    struct HudModel
    {
        Opaax::Uint32 Jumps = 0;
        float         Speed = 0.f;   // 0..1 of the mover's max — the bar reads it straight

        OPAAX_PROPERTIES(HudModel,
                         OPAAX_PROP(Jumps),
                         OPAAX_PROP(Speed))
    };

    /**
     * The HUD: a jump counter and a speed bar over the Play world. The tree is loaded from
     * UI/Hud.opaaxui and added under the GameInstance's canvas; this code names no widget. It
     * registers its model as the canvas's "Hud" source and writes it; the asset binds the fields.
     */
    class HudSubsystem final : public Opaax::WorldSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(HudSubsystem)

        static bool ShouldCreate(const Opaax::World& InWorld);

    public:
        explicit HudSubsystem(Opaax::WorldContext& InContext) : m_Context(&InContext) {}

    public:
        bool Startup() override;
        void Update(double InDeltaTime) override;
        void Shutdown() override;

    private:
        void OnJump(const Opaax::InputActionValue& InValue);

    private:
        Opaax::WorldContext* m_Context = nullptr;   // borrowed; the World owns it

        Opaax::UIWidget*       m_Root = nullptr;   // owned by the canvas until Shutdown takes it back
        HudModel               m_Model;            // read by the canvas until Shutdown removes the source
        Opaax::UIBindingHandle m_Source;           // this world's "Hud" source (the next world registers its own)
    };
}
