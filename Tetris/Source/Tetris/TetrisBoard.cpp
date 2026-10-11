#include "TetrisBoard.h"

#include <algorithm>
#include <random>

#include "Input/Mapping/InputMappingSubsystem.h"
#include "Renderer/Components/SpriteComponent.h"
#include "UI/UISubsystem.h"
#include "UI/UIWidget.h"
#include "World/Components/TransformComponent.h"
#include "World/Systems/WorldContext.h"

namespace Tetris
{
    using namespace Opaax;

    namespace
    {
        constexpr const char* PIECE_NAMES[] = { "I", "O", "T", "S", "Z", "J", "L", "Garbage", "None" };

        LinearColor ColorOf(const EPiece InPiece, const float InAlpha)
        {
            switch (InPiece)
            {
                case EPiece::I: return { 0.0f,  0.85f, 0.95f, InAlpha };
                case EPiece::O: return { 0.97f, 0.85f, 0.1f,  InAlpha };
                case EPiece::T: return { 0.65f, 0.25f, 0.9f,  InAlpha };
                case EPiece::S: return { 0.25f, 0.85f, 0.3f,  InAlpha };
                case EPiece::Z: return { 0.95f, 0.25f, 0.25f, InAlpha };
                case EPiece::J: return { 0.2f,  0.4f,  0.95f, InAlpha };
                case EPiece::L: return { 0.97f, 0.55f, 0.1f,  InAlpha };
                default:        return { 0.45f, 0.45f, 0.5f,  InAlpha };
            }
        }

        // Falling piece in front of the board's blocks, where it would land behind them.
        constexpr Int16 ORDER_GHOST   = -1;
        constexpr Int16 ORDER_BLOCK   = 0;
        constexpr Int16 ORDER_FALLING = 1;
    }

    void TetrisBoard::OnStart()
    {
        if (InputMappingSubsystem* const lActions = GetContext().Actions)
        {
            lActions->AddContextAsset(OPAAX_ID("Tetris"), OpaaxString("Input/Tetris.opaaxinputmap"));
        }

        if (UISubsystem* const lUI = GetContext().UI)
        {
            m_HudTree = lUI->MountAsset(Hud);
            if (m_HudTree != nullptr)
            {
                m_GameOver = m_HudTree->FindByName(OpaaxString("GameOver"));
                m_Paused   = m_HudTree->FindByName(OpaaxString("Paused"));
                m_Binding  = lUI->GetCanvas().Bindings().Add(OPAAX_ID("Board"), MakeBindingReader(*this));
            }
        }

        // The falling piece, its landing place and the next piece: four blocks each, kept all game.
        for (TFixedArray<Entity, 4>* lSet : { &m_Falling, &m_Ghost, &m_Preview })
        {
            for (Entity& lBlock : *lSet)
            {
                lBlock = MakeBlock();
            }
        }

        NewGame();
    }

    void TetrisBoard::OnUpdate(const float InDeltaTime)
    {
        if (WasActionStarted(OPAAX_ID("Restart")))
        {
            NewGame();
        }
        else if (WasActionStarted(OPAAX_ID("Pause")) && !m_Rules.IsGameOver())
        {
            bPaused = !bPaused;
        }

        if (!bPaused && !m_Rules.IsGameOver())
        {
            if (WasActionStarted(OPAAX_ID("RotateCW")))
            {
                m_Rules.Rotate(1);
            }
            if (WasActionStarted(OPAAX_ID("RotateCCW")))
            {
                m_Rules.Rotate(-1);
            }
            ReadShift(InDeltaTime);

            React(WasActionStarted(OPAAX_ID("HardDrop"))
                      ? m_Rules.HardDrop()
                      : m_Rules.Advance(InDeltaTime, IsActionActive(OPAAX_ID("SoftDrop"))));
        }

        Publish();
        SyncBoard();
        SyncPieces();
    }

    void TetrisBoard::OnDestroy()
    {
        if (InputMappingSubsystem* const lActions = GetContext().Actions)
        {
            lActions->RemoveContext(OPAAX_ID("Tetris"));
        }

        UISubsystem* const lUI = GetContext().UI;
        if (lUI != nullptr && m_HudTree != nullptr)
        {
            lUI->GetCanvas().Bindings().Remove(m_Binding);
            lUI->GetCanvas().Root().RemoveChild(*m_HudTree);
        }
        m_HudTree  = nullptr;
        m_GameOver = nullptr;
        m_Paused   = nullptr;

        // The blocks go with the board; at the end of the level they go with the world anyway.
        if (!IsWorldEnding())
        {
            for (Entity& lBlock : m_Cells)
            {
                if (lBlock.IsValid()) { Destroy(lBlock); }
            }
            for (TFixedArray<Entity, 4>* lSet : { &m_Falling, &m_Ghost, &m_Preview })
            {
                for (Entity& lBlock : *lSet)
                {
                    if (lBlock.IsValid()) { Destroy(lBlock); }
                }
            }
        }
    }

    void TetrisBoard::NewGame()
    {
        // Seed 0: a different order of pieces every game.
        const Uint32 lSeed = (Seed != 0) ? static_cast<Uint32>(Seed) : std::random_device{}();
        m_Rules.Reset(lSeed, StartLevel, GarbageRows, GarbageGap);

        bPaused         = false;
        m_HeldDirection = 0;

        Publish();
        SyncBoard();
        SyncPieces();
    }

    void TetrisBoard::ReadShift(const float InDeltaTime)
    {
        const bool  bLeft      = IsActionActive(OPAAX_ID("Left"));
        const bool  bRight     = IsActionActive(OPAAX_ID("Right"));
        const Int32 lDirection = (bLeft == bRight) ? 0 : (bRight ? 1 : -1);

        if (lDirection == 0)
        {
            m_HeldDirection = 0;
            return;
        }

        if (lDirection != m_HeldDirection)
        {
            m_HeldDirection = lDirection;
            m_RepeatTimer   = REPEAT_DELAY;
            m_Rules.Shift(lDirection);
            return;
        }

        m_RepeatTimer -= InDeltaTime;
        while (m_RepeatTimer <= 0.f)
        {
            m_Rules.Shift(lDirection);
            m_RepeatTimer += REPEAT_INTERVAL;
        }
    }

    void TetrisBoard::React(const StepResult& InStep) const
    {
        const OpaaxString& lSound = InStep.bGameOver          ? GameOverSound
                                    : InStep.LinesCleared > 0 ? ClearSound
                                    : InStep.bLocked          ? LockSound
                                                              : OpaaxString();
        if (!lSound.IsEmpty())
        {
            PlaySound(lSound);
        }
    }

    void TetrisBoard::Publish()
    {
        Score     = m_Rules.GetScore();
        Lines     = m_Rules.GetLines();
        Level     = m_Rules.GetLevel();
        Locked    = m_Rules.GetLocked();
        bGameOver = m_Rules.IsGameOver();
        Piece     = OpaaxString(PIECE_NAMES[static_cast<Int32>(m_Rules.GetPiece())]);
        Next      = OpaaxString(PIECE_NAMES[static_cast<Int32>(m_Rules.GetNext())]);

        PieceLeft   = TetrisRules::WIDTH;
        PieceBottom = TetrisRules::ROWS;
        for (const CellPos& lCell : m_Rules.GetPieceCells())
        {
            PieceLeft   = std::min(PieceLeft, lCell.X);
            PieceBottom = std::min(PieceBottom, lCell.Y);
        }

        if (m_GameOver != nullptr) { m_GameOver->bVisible = bGameOver; }
        if (m_Paused != nullptr)   { m_Paused->bVisible   = bPaused; }
    }

    void TetrisBoard::SyncBoard()
    {
        if (m_Rules.GetBoardRevision() == m_DrawnRevision)
        {
            return;
        }
        m_DrawnRevision = m_Rules.GetBoardRevision();

        Blocks = 0;
        for (Int32 lY = 0; lY < TetrisRules::VISIBLE_ROWS; ++lY)
        {
            for (Int32 lX = 0; lX < TetrisRules::WIDTH; ++lX)
            {
                const EPiece lCell  = m_Rules.GetCell(lX, lY);
                Entity&      lBlock = m_Cells[lY * TetrisRules::WIDTH + lX];

                if (lCell == EPiece::None)
                {
                    if (lBlock.IsValid())
                    {
                        Destroy(lBlock);
                    }
                    lBlock = Entity{};
                    continue;
                }

                if (!lBlock.IsValid())
                {
                    lBlock = MakeBlock();
                }
                Paint(lBlock, CellCentre(lX, lY), lCell, 1.f, ORDER_BLOCK);
                ++Blocks;
            }
        }
    }

    void TetrisBoard::SyncPieces()
    {
        const bool                    bPlaying = !m_Rules.IsGameOver();
        const TFixedArray<CellPos, 4> lCells   = m_Rules.GetPieceCells();
        const Int32                   lDrop    = m_Rules.GetDropDistance();

        for (Int32 lIndex = 0; lIndex < 4; ++lIndex)
        {
            const CellPos& lCell = lCells[lIndex];

            // The part of a piece still above the board is not drawn.
            if (bPlaying && lCell.Y < TetrisRules::VISIBLE_ROWS)
            {
                Paint(m_Falling[lIndex], CellCentre(lCell.X, lCell.Y), m_Rules.GetPiece(), 1.f, ORDER_FALLING);
            }
            else
            {
                Hide(m_Falling[lIndex]);
            }

            if (bPlaying && lDrop > 0 && lCell.Y - lDrop < TetrisRules::VISIBLE_ROWS)
            {
                Paint(m_Ghost[lIndex], CellCentre(lCell.X, lCell.Y - lDrop), m_Rules.GetPiece(), 0.25f, ORDER_GHOST);
            }
            else
            {
                Hide(m_Ghost[lIndex]);
            }
        }

        // The next piece, centred on NextPosition.
        const TFixedArray<CellPos, 4> lNext = TetrisRules::Shape(m_Rules.GetNext(), 0);
        Vector2F lMin{ 4.f, 4.f };
        Vector2F lMax{ 0.f, 0.f };
        for (const CellPos& lCell : lNext)
        {
            lMin = { std::min(lMin.x, static_cast<float>(lCell.X)), std::min(lMin.y, static_cast<float>(lCell.Y)) };
            lMax = { std::max(lMax.x, static_cast<float>(lCell.X)), std::max(lMax.y, static_cast<float>(lCell.Y)) };
        }
        const Vector2F lCentre = (lMin + lMax) * 0.5f;
        for (Int32 lIndex = 0; lIndex < 4; ++lIndex)
        {
            const Vector2F lOffset{ static_cast<float>(lNext[lIndex].X) - lCentre.x,
                                    static_cast<float>(lNext[lIndex].Y) - lCentre.y };
            Paint(m_Preview[lIndex], NextPosition + lOffset * CellSize, m_Rules.GetNext(), 1.f, ORDER_BLOCK);
        }
    }

    Entity TetrisBoard::MakeBlock() const
    {
        return Spawn(BlockPrefab, GetTransform().Position);
    }

    void TetrisBoard::Paint(Entity InBlock, const Vector2F& InAt, const EPiece InPiece, const float InAlpha,
                            const Int16 InOrder) const
    {
        if (!InBlock.IsValid())
        {
            return;   // the prefab did not load (logged by Spawn)
        }

        InBlock.Get<TransformComponent>().Position = InAt;
        if (SpriteComponent* const lSprite = InBlock.TryGet<SpriteComponent>())
        {
            lSprite->Color        = ColorOf(InPiece, InAlpha);
            lSprite->Size         = { CellSize - 2.f, CellSize - 2.f };
            lSprite->OrderInLayer = InOrder;
            lSprite->bVisible     = true;
        }
    }

    void TetrisBoard::Hide(Entity InBlock) const
    {
        if (InBlock.IsValid())
        {
            if (SpriteComponent* const lSprite = InBlock.TryGet<SpriteComponent>())
            {
                lSprite->bVisible = false;
            }
        }
    }

    Vector2F TetrisBoard::CellCentre(const Int32 InX, const Int32 InY) const
    {
        const Vector2F lOrigin = GetTransform().Position;
        return { lOrigin.x + (static_cast<float>(InX) - (TetrisRules::WIDTH - 1) * 0.5f) * CellSize,
                 lOrigin.y + (static_cast<float>(InY) - (TetrisRules::VISIBLE_ROWS - 1) * 0.5f) * CellSize };
    }
}
