#include "TetrisRules.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Tetris
{
    using namespace Opaax;

    namespace
    {
        // Each piece as it appears, in its box (x right, y up): 4x4 for I, 2x2 for O, 3x3 for the rest.
        constexpr CellPos SPAWN_SHAPES[7][4] = {
            { { 0, 2 }, { 1, 2 }, { 2, 2 }, { 3, 2 } },   // I
            { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } },   // O
            { { 1, 2 }, { 0, 1 }, { 1, 1 }, { 2, 1 } },   // T
            { { 1, 2 }, { 2, 2 }, { 0, 1 }, { 1, 1 } },   // S
            { { 0, 2 }, { 1, 2 }, { 1, 1 }, { 2, 1 } },   // Z
            { { 0, 2 }, { 0, 1 }, { 1, 1 }, { 2, 1 } },   // J
            { { 2, 2 }, { 0, 1 }, { 1, 1 }, { 2, 1 } },   // L
        };

        // Where a blocked turn tries the piece instead, in this order.
        constexpr CellPos KICKS[] = { { 0, 0 }, { -1, 0 }, { 1, 0 }, { 0, 1 }, { -2, 0 }, { 2, 0 } };

        // Points for one to four rows cleared at once, times the level.
        constexpr Int32 LINE_POINTS[5] = { 0, 100, 300, 500, 800 };

        Int32 BoxSize(const EPiece InPiece)
        {
            return InPiece == EPiece::I ? 4 : (InPiece == EPiece::O ? 2 : 3);
        }
    }

    TFixedArray<CellPos, 4> TetrisRules::Shape(const EPiece InPiece, const Int32 InQuarterTurns)
    {
        TFixedArray<CellPos, 4> lCells{};
        if (InPiece >= EPiece::Garbage)
        {
            return lCells;
        }

        const Int32 lBox   = BoxSize(InPiece);
        const Int32 lTurns = ((InQuarterTurns % 4) + 4) % 4;

        for (Int32 lIndex = 0; lIndex < 4; ++lIndex)
        {
            CellPos lCell = SPAWN_SHAPES[static_cast<Int32>(InPiece)][lIndex];
            for (Int32 lTurn = 0; lTurn < lTurns; ++lTurn)
            {
                // A quarter turn clockwise about the box's centre.
                lCell = CellPos{ lCell.Y, lBox - 1 - lCell.X };
            }
            lCells[lIndex] = lCell;
        }
        return lCells;
    }

    float TetrisRules::FallInterval(const Int32 InLevel)
    {
        // The usual curve: a row a second at level 1, about twenty a second by level 15.
        const Int32 lLevel = std::clamp(InLevel, 1, 20);
        return static_cast<float>(std::pow(0.8 - (lLevel - 1) * 0.007, lLevel - 1));
    }

    void TetrisRules::Reset(const Uint32 InSeed, const Int32 InStartLevel, const Int32 InGarbageRows,
                            const Int32 InGarbageGap)
    {
        m_Board.fill(EPiece::None);

        // At most up to the row pieces appear in, so the game can start.
        const Int32 lGarbageRows = std::clamp(InGarbageRows, 0, VISIBLE_ROWS - 1);
        for (Int32 lY = 0; lY < lGarbageRows; ++lY)
        {
            for (Int32 lX = 0; lX < WIDTH; ++lX)
            {
                if (lX != InGarbageGap)
                {
                    m_Board[lY * WIDTH + lX] = EPiece::Garbage;
                }
            }
        }

        m_Random     = (InSeed != 0) ? InSeed : 0x9E3779B9u;   // xorshift has no zero state
        m_BagIndex   = 7;
        m_StartLevel = std::max(InStartLevel, 1);
        m_Score      = 0;
        m_Lines      = 0;
        m_Locked     = 0;
        m_bGameOver  = false;
        ++m_BoardRevision;

        m_Next = DrawFromBag();
        SpawnNext();
    }

    bool TetrisRules::Shift(const Int32 InColumns)
    {
        if (m_bGameOver || InColumns == 0)
        {
            return false;
        }

        // Column by column: a piece does not jump over what is in the way.
        const Int32 lStep  = (InColumns > 0) ? 1 : -1;
        Int32       lMoved = 0;
        while (lMoved != InColumns && Fits(m_Piece, m_Turns, m_X + lStep, m_Y))
        {
            m_X    += lStep;
            lMoved += lStep;
        }

        if (lMoved == 0)
        {
            return false;
        }
        OnMoved();
        return true;
    }

    bool TetrisRules::Rotate(const Int32 InDirection)
    {
        if (m_bGameOver || InDirection == 0 || m_Piece == EPiece::O)
        {
            return false;
        }

        const Int32 lTurns = (m_Turns + (InDirection > 0 ? 1 : 3)) % 4;
        for (const CellPos& lKick : KICKS)
        {
            if (Fits(m_Piece, lTurns, m_X + lKick.X, m_Y + lKick.Y))
            {
                m_Turns  = lTurns;
                m_X     += lKick.X;
                m_Y     += lKick.Y;
                OnMoved();
                return true;
            }
        }
        return false;
    }

    StepResult TetrisRules::HardDrop()
    {
        StepResult lResult;
        if (m_bGameOver)
        {
            return lResult;
        }

        const Int32 lRows = GetDropDistance();
        m_Y     -= lRows;
        m_Score += 2 * lRows;

        Lock(lResult);
        return lResult;
    }

    StepResult TetrisRules::Advance(const float InSeconds, const bool bInSoftDrop)
    {
        StepResult lResult;
        if (m_bGameOver)
        {
            return lResult;
        }

        const float lInterval = FallInterval(GetLevel()) / (bInSoftDrop ? 20.f : 1.f);

        m_FallTimer += InSeconds;
        while (m_FallTimer >= lInterval && Fits(m_Piece, m_Turns, m_X, m_Y - 1))
        {
            m_FallTimer -= lInterval;
            --m_Y;
            if (bInSoftDrop)
            {
                ++m_Score;
            }

            // A row lower than ever: the piece gets its lock delay and its resets back.
            if (m_Y < m_LowestY)
            {
                m_LowestY    = m_Y;
                m_LockTimer  = 0.f;
                m_LockResets = 0;
            }
        }

        if (Fits(m_Piece, m_Turns, m_X, m_Y - 1))
        {
            return lResult;
        }

        // Resting on something: it locks once the delay has run out.
        m_FallTimer  = 0.f;
        m_LockTimer += InSeconds;
        if (m_LockTimer >= LOCK_DELAY)
        {
            Lock(lResult);
        }
        return lResult;
    }

    EPiece TetrisRules::GetCell(const Int32 InX, const Int32 InY) const
    {
        if (InX < 0 || InX >= WIDTH || InY < 0 || InY >= ROWS)
        {
            return EPiece::None;
        }
        return m_Board[InY * WIDTH + InX];
    }

    TFixedArray<CellPos, 4> TetrisRules::GetPieceCells() const
    {
        TFixedArray<CellPos, 4> lCells = Shape(m_Piece, m_Turns);
        for (CellPos& lCell : lCells)
        {
            lCell.X += m_X;
            lCell.Y += m_Y;
        }
        return lCells;
    }

    Int32 TetrisRules::GetDropDistance() const
    {
        Int32 lRows = 0;
        while (Fits(m_Piece, m_Turns, m_X, m_Y - lRows - 1))
        {
            ++lRows;
        }
        return lRows;
    }

    bool TetrisRules::Fits(const EPiece InPiece, const Int32 InTurns, const Int32 InX, const Int32 InY) const
    {
        if (InPiece >= EPiece::Garbage)
        {
            return false;
        }

        for (const CellPos& lCell : Shape(InPiece, InTurns))
        {
            const Int32 lX = InX + lCell.X;
            const Int32 lY = InY + lCell.Y;
            if (lX < 0 || lX >= WIDTH || lY < 0 || lY >= ROWS || m_Board[lY * WIDTH + lX] != EPiece::None)
            {
                return false;
            }
        }
        return true;
    }

    void TetrisRules::SpawnNext()
    {
        m_Piece = m_Next;
        m_Next  = DrawFromBag();
        m_Turns = 0;

        // Centred, its lowest cells in the top visible row.
        Int32 lLowest = 4;
        for (const CellPos& lCell : Shape(m_Piece, 0))
        {
            lLowest = std::min(lLowest, lCell.Y);
        }
        m_X = (m_Piece == EPiece::O) ? 4 : 3;
        m_Y = VISIBLE_ROWS - 1 - lLowest;

        m_LowestY    = m_Y;
        m_FallTimer  = 0.f;
        m_LockTimer  = 0.f;
        m_LockResets = 0;

        if (!Fits(m_Piece, m_Turns, m_X, m_Y))
        {
            m_bGameOver = true;
        }
    }

    void TetrisRules::Lock(StepResult& OutResult)
    {
        bool bAllHidden = true;
        for (const CellPos& lCell : GetPieceCells())
        {
            m_Board[lCell.Y * WIDTH + lCell.X] = m_Piece;
            bAllHidden = bAllHidden && lCell.Y >= VISIBLE_ROWS;
        }
        ++m_Locked;
        ++m_BoardRevision;
        OutResult.bLocked = true;

        // Scored at the level the rows were cleared at.
        const Int32 lLevel   = GetLevel();
        const Int32 lCleared = ClearFullRows();
        m_Lines += lCleared;
        m_Score += LINE_POINTS[lCleared] * lLevel;
        OutResult.LinesCleared = lCleared;

        // A piece locked wholly above the visible rows ends the game, as one that cannot appear does.
        if (bAllHidden && lCleared == 0)
        {
            m_bGameOver = true;
        }
        else
        {
            SpawnNext();
        }
        OutResult.bGameOver = m_bGameOver;
    }

    Int32 TetrisRules::ClearFullRows()
    {
        // Rows that are not full are copied down over the full ones.
        Int32 lWrite = 0;
        for (Int32 lRead = 0; lRead < ROWS; ++lRead)
        {
            bool bFull = true;
            for (Int32 lX = 0; lX < WIDTH && bFull; ++lX)
            {
                bFull = m_Board[lRead * WIDTH + lX] != EPiece::None;
            }
            if (bFull)
            {
                continue;
            }

            if (lWrite != lRead)
            {
                std::copy_n(m_Board.begin() + lRead * WIDTH, WIDTH, m_Board.begin() + lWrite * WIDTH);
            }
            ++lWrite;
        }

        std::fill(m_Board.begin() + lWrite * WIDTH, m_Board.end(), EPiece::None);
        return ROWS - lWrite;
    }

    void TetrisRules::OnMoved()
    {
        if (!Fits(m_Piece, m_Turns, m_X, m_Y - 1) && m_LockResets < MAX_LOCK_RESETS)
        {
            m_LockTimer = 0.f;
            ++m_LockResets;
        }
    }

    EPiece TetrisRules::DrawFromBag()
    {
        if (m_BagIndex >= 7)
        {
            for (Int32 lIndex = 0; lIndex < 7; ++lIndex)
            {
                m_Bag[lIndex] = static_cast<EPiece>(lIndex);
            }

            // Fisher-Yates.
            for (Int32 lIndex = 6; lIndex > 0; --lIndex)
            {
                const Int32 lOther = static_cast<Int32>(NextRandom() % static_cast<Uint32>(lIndex + 1));
                std::swap(m_Bag[lIndex], m_Bag[lOther]);
            }
            m_BagIndex = 0;
        }
        return m_Bag[m_BagIndex++];
    }

    Uint32 TetrisRules::NextRandom()
    {
        // xorshift32: the same numbers on every platform, so a seed replays the same game.
        m_Random ^= m_Random << 13;
        m_Random ^= m_Random >> 17;
        m_Random ^= m_Random << 5;
        return m_Random;
    }
}
