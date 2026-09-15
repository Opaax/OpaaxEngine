#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
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
    /** What the HUD shows — the view model the authored tree pulls from as "Hud.<field>" (UI24). */
    struct HudModel
    {
        Opaax::Uint32 Jumps = 0;
        float         Speed = 0.f;   // 0..1 of the mover's max — the bar reads it straight

        OPAAX_PROPERTIES(HudModel,
                         OPAAX_PROP(Jumps),
                         OPAAX_PROP(Speed))
    };

    /**
     * The HUD (UI U2, AUTHORED since U4, BOUND since U10): a jump counter and a speed bar over the
     * Play world.
     *
     * The tree is loaded from `UI/Hud.opaaxui` and hung under the GameInstance's persistent canvas;
     * this names NO widget. It registers its model as the canvas's "Hud" source, writes the model,
     * and the asset says which widget shows which field.
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

        Opaax::UIWidget* m_Root = nullptr;   // owned by the canvas until Shutdown takes it back
        HudModel         m_Model;            // read by the canvas until Shutdown removes the source
    };
}
