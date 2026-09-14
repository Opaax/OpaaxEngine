#pragma once

#include "Core/OpaaxTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class World;
    class UIWidget;
    class UIText;
    class UIImage;
    struct WorldContext;
    struct InputActionValue;
}

namespace Sandbox
{
    /**
     * The HUD (UI U2, AUTHORED since U4): a jump counter and a speed bar over the Play world.
     *
     * The tree is NOT built here — it is loaded from `UI/Hud.opaaxui` and hung under the
     * GameInstance's persistent canvas; this only binds the pieces it DRIVES, by name. A widget the
     * author renames goes quiet with a log line rather than silently doing nothing.
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

        Opaax::UIWidget* m_Root  = nullptr;   // owned by the canvas until Shutdown takes it back
        Opaax::UIText*   m_Jumps = nullptr;   // resolved by name out of the authored tree
        Opaax::UIImage*  m_Speed = nullptr;

        Opaax::Uint32 m_JumpCount = 0;
    };
}
