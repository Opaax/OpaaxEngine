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
     * The first HUD (UI U2): a jump counter top-left, a speed bar bottom-left, over the Play
     * world. Hangs its ONE panel under the GameInstance's persistent canvas on Startup and takes
     * it back on Shutdown, so a level swap leaves the canvas clean.
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

        Opaax::UIWidget* m_Panel = nullptr;   // owned by the canvas until Shutdown takes it back
        Opaax::UIText*   m_Jumps = nullptr;
        Opaax::UIImage*  m_Speed = nullptr;

        Opaax::Uint32 m_JumpCount = 0;
    };
}
