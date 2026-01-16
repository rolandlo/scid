#include "cbh_position.h"

namespace decoder {

PositionStack::PositionStack() { stack_.push(Lookup()); }

void PositionStack::reset() {
	ASSERT(!stack_.empty());

	while (stack_.size() > 1)
		stack_.pop();
}

unsigned PositionStack::variationLevel() const { return stack_.size() - 1; }

void PositionStack::setup() {
#define __ NULL_SQUARE
	static Pieces const StandardPosition = {
	//   e   wk  wq  wr  wb  wn  wp  -   -   bk  bq  br  bb  bn  bp
	    {__, E1, D1, A1, C1, B1, A2, __, __, E8, D8, A8, C8, B8, A7},
	    {__, __, __, H1, F1, G1, B2, __, __, __, __, H8, F8, G8, B7},
	    {__, __, __, __, __, __, C2, __, __, __, __, __, __, __, C7},
	    {__, __, __, __, __, __, D2, __, __, __, __, __, __, __, D7},
	    {__, __, __, __, __, __, E2, __, __, __, __, __, __, __, E7},
	    {__, __, __, __, __, __, F2, __, __, __, __, __, __, __, F7},
	    {__, __, __, __, __, __, G2, __, __, __, __, __, __, __, G7},
	    {__, __, __, __, __, __, H2, __, __, __, __, __, __, __, H7},
	    {__, __, __, __, __, __, __, __, __, __, __, __, __, __, __},
	    {__, __, __, __, __, __, __, __, __, __, __, __, __, __, __},
	};
#undef __
#define _ 0
	static Count const PieceCountSetup = {
	    0, 0, 0, 0, 0, 1, 1, 1, // a1 .. h1
	    0, 1, 2, 3, 4, 5, 6, 7, // a2 .. h2
	    _, _, _, _, _, _, _, _, // a3 .. h3
	    _, _, _, _, _, _, _, _, // a4 .. h4
	    _, _, _, _, _, _, _, _, // a5 .. h5
	    _, _, _, _, _, _, _, _, // a6 .. h6
	    0, 1, 2, 3, 4, 5, 6, 7, // a7 .. h7
	    0, 0, 0, 0, 0, 1, 1, 1, // a8 .. h8
	};
#undef _

	reset();

	Lookup& lookup = stack_.top();

	lookup.pos = Position::getStdStart();
	::memcpy(lookup.pieces, StandardPosition, sizeof(StandardPosition));
	::memcpy(lookup.pieceCount, PieceCountSetup, sizeof(PieceCountSetup));
}

simpleMoveT PositionStack::doNullMove() { return doMove(A1, A1, PAWN); }

simpleMoveT PositionStack::doCastling(byte offs) {
	byte from, to;

	doMove(KING, 0, offs, from, to, false);
	Lookup& lookup = stack_.top();
	Count& pieceCount = lookup.pieceCount;
	Pieces& pieces = lookup.pieces;
	Position& pos = lookup.pos;
	colorT sideToMove = pos.GetToMove();
	bool isShortCastling = from < to; // XXX only works with KxR notation

	byte rank = square_Rank(to);
	// TODO: does not work with chess 960
	byte rookFrom = square_Make(isShortCastling ? H_FYLE : A_FYLE, rank);
	byte rookTo = square_Make(isShortCastling ? F_FYLE : D_FYLE, rank);
	byte rookNum = pieceCount[rookFrom];

	pieces[rookNum][piece_Make(sideToMove, ROOK)] = rookTo;
	pieceCount[rookTo] = rookNum;

	return doMove(from, from, (offs == 2) ? KING : ROOK);
}

simpleMoveT PositionStack::doKingMove(byte offs) {
	byte from, to, captured;
	doMove(KING, 0, offs, from, to, true, &captured);
	return doMove(from, to, EMPTY);
}

simpleMoveT PositionStack::doQueenMove(byte number, byte offs) {
	byte from, to, captured;
	doMove(QUEEN, number, offs, from, to, true, &captured);
	return doMove(from, to, EMPTY);
}

simpleMoveT PositionStack::doRookMove(byte number, byte offs) {
	byte from, to, captured;
	doMove(ROOK, number, offs, from, to, true, &captured);
	return doMove(from, to, EMPTY);
}

simpleMoveT PositionStack::doBishopMove(byte number, byte offs) {
	byte from, to, captured;
	doMove(BISHOP, number, offs, from, to, true, &captured);
	return doMove(from, to, EMPTY);
}

simpleMoveT PositionStack::doKnightMove(byte number, byte offs) {
	byte from, to, captured;
	doMove(KNIGHT, number, offs, from, to, false, &captured);
	return doMove(from, to, EMPTY);
}

simpleMoveT PositionStack::doPawnOneForward(byte number) {
	byte from, to;
	colorT sideToMove = stack_.top().pos.GetToMove();
	doMove(PAWN, number, sideToMove == WHITE ? +8 : -8, from, to, false);
	return doMove(from, to, EMPTY);
}

simpleMoveT PositionStack::doPawnTwoForward(byte number) {
	byte from, to;
	colorT sideToMove = stack_.top().pos.GetToMove();
	doMove(PAWN, number, sideToMove == WHITE ? +16 : -16, from, to, false);
	return doMove(from, to, EMPTY);
}

simpleMoveT PositionStack::doCapture(byte number, byte offs) {
	byte from, to, captured;
	doMove(PAWN, number, offs, from, to, false, &captured);
	if (captured != EMPTY) {
		return doMove(from, to, EMPTY);
	}
	printf("En passant not handled yet\n");
	return simpleMoveT();
}

simpleMoveT PositionStack::doCaptureRight(byte number) {
	colorT sideToMove = stack_.top().pos.GetToMove();
	return doCapture(number, sideToMove == WHITE ? +9 : -9);
}
simpleMoveT PositionStack::doCaptureLeft(byte number) {
	colorT sideToMove = stack_.top().pos.GetToMove();
	return doCapture(number, sideToMove == WHITE ? +7 : -7);
}

simpleMoveT PositionStack::doMove(byte from, byte to, byte promoted) {
	Position& pos = stack_.top().pos;
	simpleMoveT sm;
	pos.makeMove(from, to, promoted, sm);
	pos.DoSimpleMove(sm);
	return sm;
}

void PositionStack::handleCapture(Pieces& pieces, Count& pieceCount, byte to,
                                  byte piece) {
	if (piece == EMPTY)
		return;

	byte pieceNum = pieceCount[to];

	pieces[pieceNum][piece] = NULL_SQUARE;

	if (piece_Type(piece) == PAWN)
		return;

	for (unsigned i = 0; i < 10; ++i) {
		byte square = pieces[i][piece];

		if (square != NULL_SQUARE) {
			byte number = pieceCount[square];

			if (number > pieceNum) {
				pieces[number - 1][piece] = pieces[number][piece];
				pieces[number][piece] = NULL_SQUARE;
				--pieceCount[square];
			}
		} else if (i != pieceNum) {
			return;
		}
	}
}

void PositionStack::doMove(pieceT pieceType, byte number, byte offs, byte& from,
                           byte& to, bool dontWrap, byte* captured) {
	Lookup& lookup = stack_.top();
	Pieces& pieces = lookup.pieces;
	Count& pieceCount = lookup.pieceCount;
	Position pos = lookup.pos;

	colorT sideToMove = pos.GetToMove();
	byte& square = pieces[number][pieceType | (sideToMove << 3)];

	if (dontWrap && square_Fyle(square) + (offs & 0x7) > H_FYLE)
		offs -= 8;

	from = square;
	to = (from + offs) & 63;

	if (captured) // if we have a memory address to save the type of a
	              // potentially captured piece to
	{
		pieceT capturedPiece = pos.GetPiece(to);
		handleCapture(pieces, pieceCount, to, capturedPiece);
		*captured = capturedPiece;
	}
	square = to;
	pieceCount[to] = number;
}
} // namespace decoder