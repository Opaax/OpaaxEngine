#pragma once

#include "Core/OpaaxTypes.h"

namespace Tetris
{
    /** The seven pieces, the grey rows a game can start with, and an empty cell. */
    enum class EPiece : Opaax::Uint8 { I, O, T, S, Z, J, L, Garbage, None };

    /** A board cell: column X from the left, row Y from the bottom. */
    struct CellPos
    {
        Opaax::Int32 X = 0;
        Opaax::Int32 Y = 0;
    };

    /** What a step of the game did, for sounds and effects. */
    struct StepResult
    {
        bool         bLocked      = false;
        Opaax::Int32 LinesCleared = 0;
        bool         bGameOver    = false;
    };

    // =============================================================================
    // TetrisRules — a game of Tetris without drawing or input: the board, the falling piece, gravity,
    //   the lock delay, line clears, scoring and the order of pieces (shuffled bags of seven).
    //   Rows count up from the bottom. A piece appears in the top visible row; the rows above the
    //   visible twenty only hold the part of a piece that has just appeared.
    // =============================================================================
    class TetrisRules
    {
    public:
        static constexpr Opaax::Int32 WIDTH           = 10;
        static constexpr Opaax::Int32 VISIBLE_ROWS    = 20;
        static constexpr Opaax::Int32 ROWS            = 24;
        static constexpr float        LOCK_DELAY      = 0.5f;
        static constexpr Opaax::Int32 MAX_LOCK_RESETS = 15;

        /** The four cells of InPiece turned InQuarterTurns clockwise, in its box (bottom-left origin). */
        static Opaax::TFixedArray<CellPos, 4> Shape(EPiece InPiece, Opaax::Int32 InQuarterTurns);

        /** Seconds a piece takes to fall one row at InLevel. */
        static float FallInterval(Opaax::Int32 InLevel);

        /**
         * A new game. The InGarbageRows bottom rows start full except column InGarbageGap. The same seed
         * gives the same pieces.
         */
        void Reset(Opaax::Uint32 InSeed, Opaax::Int32 InStartLevel, Opaax::Int32 InGarbageRows = 0,
                   Opaax::Int32 InGarbageGap = 0);

        // =============================================================================
        // Moves — each answers whether the piece moved. Nothing moves once the game is over.
        // =============================================================================

        /** InColumns to the right (negative: to the left). */
        bool Shift(Opaax::Int32 InColumns);

        /** A quarter turn clockwise (1) or counter-clockwise (-1), nudged sideways or up when blocked. */
        bool Rotate(Opaax::Int32 InDirection);

        /** Drops the piece to the bottom and locks it: two points a row. */
        StepResult HardDrop();

        /** Gravity and the lock delay over InSeconds. A soft drop falls 20 times faster, a point a row. */
        StepResult Advance(float InSeconds, bool bInSoftDrop);

        // =============================================================================
        // State
        // =============================================================================

        /** What fills a cell; None outside the board. */
        EPiece GetCell(Opaax::Int32 InX, Opaax::Int32 InY) const;

        /** The falling piece's four cells on the board. */
        Opaax::TFixedArray<CellPos, 4> GetPieceCells() const;

        /** Rows the falling piece can still fall. */
        Opaax::Int32 GetDropDistance() const;

        EPiece       GetPiece()    const noexcept { return m_Piece; }
        EPiece       GetNext()     const noexcept { return m_Next; }
        Opaax::Int32 GetScore()    const noexcept { return m_Score; }
        Opaax::Int32 GetLines()    const noexcept { return m_Lines; }
        Opaax::Int32 GetLevel()    const noexcept { return m_StartLevel + m_Lines / 10; }
        Opaax::Int32 GetLocked()   const noexcept { return m_Locked; }
        bool         IsGameOver()  const noexcept { return m_bGameOver; }

        /** Changes each time a cell of the board changes, so a view redraws the board only then. */
        Opaax::Uint32 GetBoardRevision() const noexcept { return m_BoardRevision; }

    private:
        bool Fits(EPiece InPiece, Opaax::Int32 InTurns, Opaax::Int32 InX, Opaax::Int32 InY) const;

        /** The next piece enters at the top; the game is over when it does not fit. */
        void SpawnNext();

        /** Writes the piece into the board, clears full rows, scores, and brings the next piece. */
        void Lock(StepResult& OutResult);

        Opaax::Int32 ClearFullRows();

        /** After a move of a piece resting on something: it gets its lock delay back, a few times. */
        void OnMoved();

        EPiece        DrawFromBag();
        Opaax::Uint32 NextRandom();

        Opaax::TFixedArray<EPiece, WIDTH * ROWS> m_Board{};
        Opaax::TFixedArray<EPiece, 7>            m_Bag{};
        Opaax::Int32                             m_BagIndex = 7;
        Opaax::Uint32                            m_Random   = 1;

        EPiece       m_Piece = EPiece::None;
        EPiece       m_Next  = EPiece::None;
        Opaax::Int32 m_Turns = 0;
        Opaax::Int32 m_X     = 0;
        Opaax::Int32 m_Y     = 0;

        float        m_FallTimer  = 0.f;
        float        m_LockTimer  = 0.f;
        Opaax::Int32 m_LockResets = 0;
        Opaax::Int32 m_LowestY    = 0;   // the lowest row this piece has reached

        Opaax::Int32  m_StartLevel    = 1;
        Opaax::Int32  m_Score         = 0;
        Opaax::Int32  m_Lines         = 0;
        Opaax::Int32  m_Locked        = 0;
        bool          m_bGameOver     = false;
        Opaax::Uint32 m_BoardRevision = 0;
    };
}
