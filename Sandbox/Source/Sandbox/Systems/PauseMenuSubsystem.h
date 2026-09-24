#pragma once

#include "Core/OpaaxTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class World;
    class UIWidget;
    class UIButton;
    struct WorldContext;
    struct InputActionValue;
}

namespace Sandbox
{
    /**
     * The first menu (UI U3): a "Menu" button on the HUD (GameAndUI — a click opens it while the
     * game keeps its keys), and a dimmed modal with Resume (UIOnly — the mapping is muted, Escape
     * closes it through the focused panel). One owner for the menu; the world keeps running
     * underneath, by design for now.
     *
     * The modal's contents are `UI/PauseMenu.opaaxui` (since U9), hung under a code-built root that
     * handles Escape; the two buttons are bound by name, the HUD's shape.
     */
    class PauseMenuSubsystem final : public Opaax::WorldSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(PauseMenuSubsystem)

        static bool ShouldCreate(const Opaax::World& InWorld);

    public:
        explicit PauseMenuSubsystem(Opaax::WorldContext& InContext) : m_Context(&InContext) {}

    public:
        bool Startup() override;

        /** The fade: Opacity chases 1 while open and 0 while closing; the panel hides when it gets there. */
        void Update(double InDeltaTime) override;

        void Shutdown() override;

        void Open();
        void Close();
        bool IsOpen() const noexcept { return m_bOpen; }

    private:
        void OnMenuToggle(const Opaax::InputActionValue& InValue);

        /** Main ⇄ PhysicsTest through the deferred request — the level swap the cover is for (UI21). */
        void NextLevel();

    private:
        Opaax::WorldContext* m_Context = nullptr;   // borrowed; the World owns it

        Opaax::UIButton* m_MenuButton = nullptr;   // owned by the canvas until Shutdown
        Opaax::UIWidget* m_Menu       = nullptr;   // the modal, hidden while closed
        bool             m_bOpen      = false;
        Opaax::Uint32    m_Opens      = 0;
    };
}
