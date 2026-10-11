#pragma once

#include "Engine/Registries/AutoRegistration.h"
#include "UI/UIBinding.h"
#include "World/Behaviour/Behaviour.h"

#include "TetrisRules.h"

namespace Opaax
{
    class UIWidget;
}

namespace Tetris
{
    // =============================================================================
    // TetrisBoard — plays Tetris on its entity: the board is centred on it, CellSize per cell. Every
    //   block is spawned from BlockPrefab (a sprite, coloured per piece); the next piece is shown at
    //   NextPosition. The Hud canvas shows Score, Lines and Level through the "Board" binding, and its
    //   GameOver and Paused widgets when they apply.
    //   Keys (Input/Tetris.opaaxinputmap): Left and Right move, Up turns, Z turns back, Down drops
    //   faster, Space drops at once, P pauses, Enter starts a new game.
    // =============================================================================
    class TetrisBoard : public Opaax::Behaviour
    {
    public:
        /** The order of pieces; 0 gives a new order every game. */
        Opaax::Int32       Seed         = 0;
        Opaax::Int32       StartLevel   = 1;

        /** Grey rows the game starts with, full but for column GarbageGap. */
        Opaax::Int32       GarbageRows  = 0;
        Opaax::Int32       GarbageGap   = 0;

        float              CellSize     = 30.f;
        Opaax::Vector2F    NextPosition = { 270.f, 180.f };
        Opaax::OpaaxString BlockPrefab  = "Prefabs/Block.opaaxprefab";
        Opaax::OpaaxString Hud          = "UI/Hud.opaaxui";

        // Played when a piece locks, when rows clear, when the game ends (empty: silent).
        Opaax::OpaaxString LockSound     = "Audio/Lock.wav";
        Opaax::OpaaxString ClearSound    = "Audio/Clear.wav";
        Opaax::OpaaxString GameOverSound = "Audio/GameOver.wav";

        // What the game shows: the HUD reads these, a test can too.
        Opaax::Int32       Score     = 0;
        Opaax::Int32       Lines     = 0;
        Opaax::Int32       Level     = 1;
        Opaax::Int32       Locked    = 0;
        Opaax::Int32       Blocks    = 0;
        bool               bPaused   = false;
        bool               bGameOver = false;
        Opaax::OpaaxString Piece;
        Opaax::OpaaxString Next;

        /** The falling piece's leftmost column and lowest row. */
        Opaax::Int32       PieceLeft   = 0;
        Opaax::Int32       PieceBottom = 0;

        OPAAX_PROPERTIES(TetrisBoard,
                         OPAAX_PROP(Seed), OPAAX_PROP(StartLevel).SetRange(1.f, 20.f),
                         OPAAX_PROP(GarbageRows).SetRange(0.f, 19.f), OPAAX_PROP(GarbageGap).SetRange(0.f, 9.f),
                         OPAAX_PROP(CellSize).SetRange(4.f, 200.f), OPAAX_PROP(NextPosition),
                         OPAAX_PROP(BlockPrefab), OPAAX_PROP(Hud), OPAAX_PROP(LockSound), OPAAX_PROP(ClearSound),
                         OPAAX_PROP(GameOverSound),
                         OPAAX_PROP(Score), OPAAX_PROP(Lines), OPAAX_PROP(Level), OPAAX_PROP(Locked),
                         OPAAX_PROP(Blocks), OPAAX_PROP(bPaused), OPAAX_PROP(bGameOver), OPAAX_PROP(Piece),
                         OPAAX_PROP(Next), OPAAX_PROP(PieceLeft), OPAAX_PROP(PieceBottom))

        void OnStart() override;
        void OnUpdate(float InDeltaTime) override;
        void OnDestroy() override;

    private:
        void NewGame();

        /** Left and Right: one column at the press, then repeated while held. */
        void ReadShift(float InDeltaTime);

        /** Sounds for what a step did. */
        void React(const StepResult& InStep) const;

        /** Copies the game's state into the fields above, and shows or hides the HUD's messages. */
        void Publish();

        /** Locked blocks: spawned and destroyed to match the board, only when it changed. */
        void SyncBoard();

        /** The falling piece, where it would land, and the next piece. */
        void SyncPieces();

        Opaax::Entity   MakeBlock() const;
        void            Paint(Opaax::Entity InBlock, const Opaax::Vector2F& InAt, EPiece InPiece, float InAlpha,
                              Opaax::Int16 InOrder) const;
        void            Hide(Opaax::Entity InBlock) const;
        Opaax::Vector2F CellCentre(Opaax::Int32 InX, Opaax::Int32 InY) const;

        static constexpr float REPEAT_DELAY    = 0.17f;
        static constexpr float REPEAT_INTERVAL = 0.05f;

        TetrisRules   m_Rules;
        Opaax::Uint32 m_DrawnRevision = ~0u;

        Opaax::TFixedArray<Opaax::Entity, TetrisRules::WIDTH * TetrisRules::VISIBLE_ROWS> m_Cells{};
        Opaax::TFixedArray<Opaax::Entity, 4> m_Falling{};
        Opaax::TFixedArray<Opaax::Entity, 4> m_Ghost{};
        Opaax::TFixedArray<Opaax::Entity, 4> m_Preview{};

        Opaax::Int32 m_HeldDirection = 0;
        float        m_RepeatTimer   = 0.f;

        Opaax::UIWidget*       m_HudTree  = nullptr;   // owned by the game's canvas
        Opaax::UIWidget*       m_GameOver = nullptr;
        Opaax::UIWidget*       m_Paused   = nullptr;
        Opaax::UIBindingHandle m_Binding;
    };

    OPAAX_REGISTER_BEHAVIOUR(TetrisBoard);
}
