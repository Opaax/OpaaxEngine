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
     * The pause menu: a "Menu" button on the HUD (GameAndUI: the game keeps its keys) and a dimmed modal
     * with Resume (UIOnly: the mapping is muted, Escape closes it). The world keeps running underneath.
     * The modal's content is UI/PauseMenu.opaaxui, under a code-built root that handles Escape; the
     * buttons are found by name.
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

        /** The fade: Opacity moves toward 1 while open and 0 while closing; the panel hides at 0. */
        void Update(double InDeltaTime) override;

        void Shutdown() override;

        void Open();
        void Close();
        bool IsOpen() const noexcept { return m_bOpen; }

    private:
        void OnMenuToggle(const Opaax::InputActionValue& InValue);

        /** Main <-> PhysicsTest through a deferred level request. */
        void NextLevel();

    private:
        Opaax::WorldContext* m_Context = nullptr;   // borrowed; the World owns it

        Opaax::UIButton* m_MenuButton = nullptr;   // owned by the canvas until Shutdown
        Opaax::UIWidget* m_Menu       = nullptr;   // the modal, hidden while closed
        bool             m_bOpen      = false;
        Opaax::Uint32    m_Opens      = 0;
    };
}
