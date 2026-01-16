/*
 * Copyright (C) 2026  Roland Lötscher
 *
 * This file is part of SCID (Shane's Chess Information Database).
 *
 * SCID is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * SCID is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SCID. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/** @file
 * Implements a parser that converts .cbg records into SCID's Game objects.
 */

#include "filebuf.h"
#include "game.h"
#include <string>
#include <string_view>
#include <vector>

static constexpr auto GAME_HEADER_SIZE = 26;

static byte MoveNumberLookup[256] = {
    0xa2, 0x95, 0x43, 0xf5, 0xc1, 0x3d, 0x4a, 0x6c, //   0 -   7
    0x53, 0x83, 0xcc, 0x7c, 0xff, 0xae, 0x68, 0xad, //   8 -  15
    0xd1, 0x92, 0x8b, 0x8d, 0x35, 0x81, 0x5e, 0x74, //  16 -  23
    0x26, 0x8e, 0xab, 0xca, 0xfd, 0x9a, 0xf3, 0xa0, //  24 -  31
    0xa5, 0x15, 0xfc, 0xb1, 0x1e, 0xed, 0x30, 0xea, //  32 -  39
    0x22, 0xeb, 0xa7, 0xcd, 0x4e, 0x6f, 0x2e, 0x24, //  40 -  47
    0x32, 0x94, 0x41, 0x8c, 0x6e, 0x58, 0x82, 0x50, //  48 -  55
    0xbb, 0x02, 0x8a, 0xd8, 0xfa, 0x60, 0xde, 0x52, //  56 -  63
    0xba, 0x46, 0xac, 0x29, 0x9d, 0xd7, 0xdf, 0x08, //  64 -  71
    0x21, 0x01, 0x66, 0xa3, 0xf1, 0x19, 0x27, 0xb5, //  72 -  79
    0x91, 0xd5, 0x42, 0x0e, 0xb4, 0x4c, 0xd9, 0x18, //  80 -  87
    0x5f, 0xbc, 0x25, 0xa6, 0x96, 0x04, 0x56, 0x6a, //  88 -  95
    0xaa, 0x33, 0x1c, 0x2b, 0x73, 0xf0, 0xdd, 0xa4, //  96 - 103
    0x37, 0xd3, 0xc5, 0x10, 0xbf, 0x5a, 0x23, 0x34, // 104 - 111
    0x75, 0x5b, 0xb8, 0x55, 0xd2, 0x6b, 0x09, 0x3a, // 112 - 119
    0x57, 0x12, 0xb3, 0x77, 0x48, 0x85, 0x9b, 0x0f, // 120 - 127
    0x9e, 0xc7, 0xc8, 0xa1, 0x7f, 0x7a, 0xc0, 0xbd, // 128 - 135
    0x31, 0x6d, 0xf6, 0x3e, 0xc3, 0x11, 0x71, 0xce, // 136 - 143
    0x7d, 0xda, 0xa8, 0x54, 0x90, 0x97, 0x1f, 0x44, // 144 - 151
    0x40, 0x16, 0xc9, 0xe3, 0x2c, 0xcb, 0x84, 0xec, // 152 - 159
    0x9f, 0x3f, 0x5c, 0xe6, 0x76, 0x0b, 0x3c, 0x20, // 160 - 167
    0xb7, 0x36, 0x00, 0xdc, 0xe7, 0xf9, 0x4f, 0xf7, // 168 - 175
    0xaf, 0x06, 0x07, 0xe0, 0x1a, 0x0a, 0xa9, 0x4b, // 176 - 183
    0x0c, 0xd6, 0x63, 0x87, 0x89, 0x1d, 0x13, 0x1b, // 184 - 191
    0xe4, 0x70, 0x05, 0x47, 0x67, 0x7b, 0x2f, 0xee, // 192 - 199
    0xe2, 0xe8, 0x98, 0x0d, 0xef, 0xcf, 0xc4, 0xf4, // 200 - 207
    0xfb, 0xb0, 0x17, 0x99, 0x64, 0xf2, 0xd4, 0x2a, // 208 - 215
    0x03, 0x4d, 0x78, 0xc6, 0xfe, 0x65, 0x86, 0x88, // 216 - 223
    0x79, 0x45, 0x3b, 0xe5, 0x49, 0x8f, 0x2d, 0xb9, // 224 - 231
    0xbe, 0x62, 0x93, 0x14, 0xe9, 0xd0, 0x38, 0x9c, // 232 - 239
    0xb2, 0xc2, 0x59, 0x5d, 0xb6, 0x72, 0x51, 0xf8, // 240 - 247
    0x28, 0x7e, 0x61, 0x39, 0xe1, 0xdb, 0x69, 0x80, // 248 - 255
};

std::string doNullMove() { return "NullMove"; }
std::string doKingMove(int x, int y) {
	return "King move " + std::to_string(x) + "/" + std::to_string(y);
}
std::string doCastling(byte b) {
	return "Castle " + (b == 2) ? std::string("kingside")
	                            : std::string("queenside");
}
std::string doQueenMove(int q, int x, int y) {
	return "Queen " + std::to_string(q + 1) + " move " + std::to_string(x) +
	       "/" + std::to_string(y);
}
std::string doRookMove(int q, int x, int y) {
	{
		return "Rook " + std::to_string(q + 1) + " move " + std::to_string(x) +
		       "/" + std::to_string(y);
	}
}
std::string doBishopMove(int q, int x, int y) {
	{
		return "Bishop " + std::to_string(q + 1) + " move " +
		       std::to_string(x) + "/" + std::to_string(y);
	}
}
std::string doKnightMove(int q, int x, int y) {
	{
		return "Knight " + std::to_string(q + 1) + " move " +
		       std::to_string(x) + "/" + std::to_string(y);
	}
}
std::string doPawnOneForward(int q) {
	return "Pawn " + std::to_string(q + 1) + " moves one square";
}
std::string doPawnTwoForward(int q) {
	return "Pawn " + std::to_string(q + 1) + " moves two squares";
}
std::string doCaptureLeft(int q) {
	return "Pawn " + std::to_string(q + 1) + " captures left";
}

std::string doCaptureRight(int q) {
	return "Pawn " + std::to_string(q + 1) + " captures right";
}

std::string decodeMove(int move_number) {

	std::string move = "multibyte";
	std::string Token_Skip = "skip";
	std::string Token_Push = "push";
	std::string Token_Pop = "pop";

	switch (move_number) {
#define OFFSET(x, y) x, y

	// Null move ###########################
	case 0x00:
		move = doNullMove();
		break;

	// King ################################
	case 0x01:
		move = doKingMove(OFFSET(0, 1));
		break;
	case 0x02:
		move = doKingMove(OFFSET(1, 1));
		break;
	case 0x03:
		move = doKingMove(OFFSET(1, 0));
		break;
	case 0x04:
		move = doKingMove(OFFSET(1, 7));
		break;
	case 0x05:
		move = doKingMove(OFFSET(0, 7));
		break;
	case 0x06:
		move = doKingMove(OFFSET(7, 7));
		break;
	case 0x07:
		move = doKingMove(OFFSET(7, 0));
		break;
	case 0x08:
		move = doKingMove(OFFSET(7, 1));
		break;
	case 0x09:
		move = doCastling(byte(+2));
		break;
	case 0x0a:
		move = doCastling(byte(-2));
		break;

	// First Queen #########################
	case 0x0b:
		move = doQueenMove(0, OFFSET(0, 1));
		break;
	case 0x0c:
		move = doQueenMove(0, OFFSET(0, 2));
		break;
	case 0x0d:
		move = doQueenMove(0, OFFSET(0, 3));
		break;
	case 0x0e:
		move = doQueenMove(0, OFFSET(0, 4));
		break;
	case 0x0f:
		move = doQueenMove(0, OFFSET(0, 5));
		break;
	case 0x10:
		move = doQueenMove(0, OFFSET(0, 6));
		break;
	case 0x11:
		move = doQueenMove(0, OFFSET(0, 7));
		break;
	case 0x12:
		move = doQueenMove(0, OFFSET(1, 0));
		break;
	case 0x13:
		move = doQueenMove(0, OFFSET(2, 0));
		break;
	case 0x14:
		move = doQueenMove(0, OFFSET(3, 0));
		break;
	case 0x15:
		move = doQueenMove(0, OFFSET(4, 0));
		break;
	case 0x16:
		move = doQueenMove(0, OFFSET(5, 0));
		break;
	case 0x17:
		move = doQueenMove(0, OFFSET(6, 0));
		break;
	case 0x18:
		move = doQueenMove(0, OFFSET(7, 0));
		break;
	case 0x19:
		move = doQueenMove(0, OFFSET(1, 1));
		break;
	case 0x1a:
		move = doQueenMove(0, OFFSET(2, 2));
		break;
	case 0x1b:
		move = doQueenMove(0, OFFSET(3, 3));
		break;
	case 0x1c:
		move = doQueenMove(0, OFFSET(4, 4));
		break;
	case 0x1d:
		move = doQueenMove(0, OFFSET(5, 5));
		break;
	case 0x1e:
		move = doQueenMove(0, OFFSET(6, 6));
		break;
	case 0x1f:
		move = doQueenMove(0, OFFSET(7, 7));
		break;
	case 0x20:
		move = doQueenMove(0, OFFSET(1, 7));
		break;
	case 0x21:
		move = doQueenMove(0, OFFSET(2, 6));
		break;
	case 0x22:
		move = doQueenMove(0, OFFSET(3, 5));
		break;
	case 0x23:
		move = doQueenMove(0, OFFSET(4, 4));
		break;
	case 0x24:
		move = doQueenMove(0, OFFSET(5, 3));
		break;
	case 0x25:
		move = doQueenMove(0, OFFSET(6, 2));
		break;
	case 0x26:
		move = doQueenMove(0, OFFSET(7, 1));
		break;

	// First Rook ##########################
	case 0x27:
		move = doRookMove(0, OFFSET(0, 1));
		break;
	case 0x28:
		move = doRookMove(0, OFFSET(0, 2));
		break;
	case 0x29:
		move = doRookMove(0, OFFSET(0, 3));
		break;
	case 0x2a:
		move = doRookMove(0, OFFSET(0, 4));
		break;
	case 0x2b:
		move = doRookMove(0, OFFSET(0, 5));
		break;
	case 0x2c:
		move = doRookMove(0, OFFSET(0, 6));
		break;
	case 0x2d:
		move = doRookMove(0, OFFSET(0, 7));
		break;
	case 0x2e:
		move = doRookMove(0, OFFSET(1, 0));
		break;
	case 0x2f:
		move = doRookMove(0, OFFSET(2, 0));
		break;
	case 0x30:
		move = doRookMove(0, OFFSET(3, 0));
		break;
	case 0x31:
		move = doRookMove(0, OFFSET(4, 0));
		break;
	case 0x32:
		move = doRookMove(0, OFFSET(5, 0));
		break;
	case 0x33:
		move = doRookMove(0, OFFSET(6, 0));
		break;
	case 0x34:
		move = doRookMove(0, OFFSET(7, 0));
		break;

	// Second Rook #########################
	case 0x35:
		move = doRookMove(1, OFFSET(0, 1));
		break;
	case 0x36:
		move = doRookMove(1, OFFSET(0, 2));
		break;
	case 0x37:
		move = doRookMove(1, OFFSET(0, 3));
		break;
	case 0x38:
		move = doRookMove(1, OFFSET(0, 4));
		break;
	case 0x39:
		move = doRookMove(1, OFFSET(0, 5));
		break;
	case 0x3a:
		move = doRookMove(1, OFFSET(0, 6));
		break;
	case 0x3b:
		move = doRookMove(1, OFFSET(0, 7));
		break;
	case 0x3c:
		move = doRookMove(1, OFFSET(1, 0));
		break;
	case 0x3d:
		move = doRookMove(1, OFFSET(2, 0));
		break;
	case 0x3e:
		move = doRookMove(1, OFFSET(3, 0));
		break;
	case 0x3f:
		move = doRookMove(1, OFFSET(4, 0));
		break;
	case 0x40:
		move = doRookMove(1, OFFSET(5, 0));
		break;
	case 0x41:
		move = doRookMove(1, OFFSET(6, 0));
		break;
	case 0x42:
		move = doRookMove(1, OFFSET(7, 0));
		break;

	// First Bishop ########################
	case 0x43:
		move = doBishopMove(0, OFFSET(1, 1));
		break;
	case 0x44:
		move = doBishopMove(0, OFFSET(2, 2));
		break;
	case 0x45:
		move = doBishopMove(0, OFFSET(3, 3));
		break;
	case 0x46:
		move = doBishopMove(0, OFFSET(4, 4));
		break;
	case 0x47:
		move = doBishopMove(0, OFFSET(5, 5));
		break;
	case 0x48:
		move = doBishopMove(0, OFFSET(6, 6));
		break;
	case 0x49:
		move = doBishopMove(0, OFFSET(7, 7));
		break;
	case 0x4a:
		move = doBishopMove(0, OFFSET(1, 7));
		break;
	case 0x4b:
		move = doBishopMove(0, OFFSET(2, 6));
		break;
	case 0x4c:
		move = doBishopMove(0, OFFSET(3, 5));
		break;
	case 0x4d:
		move = doBishopMove(0, OFFSET(4, 4));
		break;
	case 0x4e:
		move = doBishopMove(0, OFFSET(5, 3));
		break;
	case 0x4f:
		move = doBishopMove(0, OFFSET(6, 2));
		break;
	case 0x50:
		move = doBishopMove(0, OFFSET(7, 1));
		break;

	// Second Bishop #######################
	case 0x51:
		move = doBishopMove(1, OFFSET(1, 1));
		break;
	case 0x52:
		move = doBishopMove(1, OFFSET(2, 2));
		break;
	case 0x53:
		move = doBishopMove(1, OFFSET(3, 3));
		break;
	case 0x54:
		move = doBishopMove(1, OFFSET(4, 4));
		break;
	case 0x55:
		move = doBishopMove(1, OFFSET(5, 5));
		break;
	case 0x56:
		move = doBishopMove(1, OFFSET(6, 6));
		break;
	case 0x57:
		move = doBishopMove(1, OFFSET(7, 7));
		break;
	case 0x58:
		move = doBishopMove(1, OFFSET(1, 7));
		break;
	case 0x59:
		move = doBishopMove(1, OFFSET(2, 6));
		break;
	case 0x5a:
		move = doBishopMove(1, OFFSET(3, 5));
		break;
	case 0x5b:
		move = doBishopMove(1, OFFSET(4, 4));
		break;
	case 0x5c:
		move = doBishopMove(1, OFFSET(5, 3));
		break;
	case 0x5d:
		move = doBishopMove(1, OFFSET(6, 2));
		break;
	case 0x5e:
		move = doBishopMove(1, OFFSET(7, 1));
		break;

	// First Knight ########################
	case 0x5f:
		move = doKnightMove(0, OFFSET(+2, +1));
		break;
	case 0x60:
		move = doKnightMove(0, OFFSET(+1, +2));
		break;
	case 0x61:
		move = doKnightMove(0, OFFSET(-1, +2));
		break;
	case 0x62:
		move = doKnightMove(0, OFFSET(-2, +1));
		break;
	case 0x63:
		move = doKnightMove(0, OFFSET(-2, -1));
		break;
	case 0x64:
		move = doKnightMove(0, OFFSET(-1, -2));
		break;
	case 0x65:
		move = doKnightMove(0, OFFSET(+1, -2));
		break;
	case 0x66:
		move = doKnightMove(0, OFFSET(+2, -1));
		break;

	// Second Knight #######################
	case 0x67:
		move = doKnightMove(1, OFFSET(+2, +1));
		break;
	case 0x68:
		move = doKnightMove(1, OFFSET(+1, +2));
		break;
	case 0x69:
		move = doKnightMove(1, OFFSET(-1, +2));
		break;
	case 0x6a:
		move = doKnightMove(1, OFFSET(-2, +1));
		break;
	case 0x6b:
		move = doKnightMove(1, OFFSET(-2, -1));
		break;
	case 0x6c:
		move = doKnightMove(1, OFFSET(-1, -2));
		break;
	case 0x6d:
		move = doKnightMove(1, OFFSET(+1, -2));
		break;
	case 0x6e:
		move = doKnightMove(1, OFFSET(+2, -1));
		break;

	// a2/a7 Pawn ##########################
	case 0x6f:
		move = doPawnOneForward(0);
		break;
	case 0x70:
		move = doPawnTwoForward(0);
		break;
	case 0x71:
		move = doCaptureRight(0);
		break;
	case 0x72:
		move = doCaptureLeft(0);
		break;

	// b2/b7 Pawn ##########################
	case 0x73:
		move = doPawnOneForward(1);
		break;
	case 0x74:
		move = doPawnTwoForward(1);
		break;
	case 0x75:
		move = doCaptureRight(1);
		break;
	case 0x76:
		move = doCaptureLeft(1);
		break;

	// c2/c7 Pawn ##########################
	case 0x77:
		move = doPawnOneForward(2);
		break;
	case 0x78:
		move = doPawnTwoForward(2);
		break;
	case 0x79:
		move = doCaptureRight(2);
		break;
	case 0x7a:
		move = doCaptureLeft(2);
		break;

	// d2/d7 Pawn ##########################
	case 0x7b:
		move = doPawnOneForward(3);
		break;
	case 0x7c:
		move = doPawnTwoForward(3);
		break;
	case 0x7d:
		move = doCaptureRight(3);
		break;
	case 0x7e:
		move = doCaptureLeft(3);
		break;

	// e2/e7 Pawn ##########################
	case 0x7f:
		move = doPawnOneForward(4);
		break;
	case 0x80:
		move = doPawnTwoForward(4);
		break;
	case 0x81:
		move = doCaptureRight(4);
		break;
	case 0x82:
		move = doCaptureLeft(4);
		break;

	// f2/f7 Pawn ##########################
	case 0x83:
		move = doPawnOneForward(5);
		break;
	case 0x84:
		move = doPawnTwoForward(5);
		break;
	case 0x85:
		move = doCaptureRight(5);
		break;
	case 0x86:
		move = doCaptureLeft(5);
		break;

	// g2/g7 Pawn ##########################
	case 0x87:
		move = doPawnOneForward(6);
		break;
	case 0x88:
		move = doPawnTwoForward(6);
		break;
	case 0x89:
		move = doCaptureRight(6);
		break;
	case 0x8a:
		move = doCaptureLeft(6);
		break;

	// h2/h7 Pawn ##########################
	case 0x8b:
		move = doPawnOneForward(7);
		break;
	case 0x8c:
		move = doPawnTwoForward(7);
		break;
	case 0x8d:
		move = doCaptureRight(7);
		break;
	case 0x8e:
		move = doCaptureLeft(7);
		break;

	// Second Queen #########################
	case 0x8f:
		move = doQueenMove(1, OFFSET(0, 1));
		break;
	case 0x90:
		move = doQueenMove(1, OFFSET(0, 2));
		break;
	case 0x91:
		move = doQueenMove(1, OFFSET(0, 3));
		break;
	case 0x92:
		move = doQueenMove(1, OFFSET(0, 4));
		break;
	case 0x93:
		move = doQueenMove(1, OFFSET(0, 5));
		break;
	case 0x94:
		move = doQueenMove(1, OFFSET(0, 6));
		break;
	case 0x95:
		move = doQueenMove(1, OFFSET(0, 7));
		break;
	case 0x96:
		move = doQueenMove(1, OFFSET(1, 0));
		break;
	case 0x97:
		move = doQueenMove(1, OFFSET(2, 0));
		break;
	case 0x98:
		move = doQueenMove(1, OFFSET(3, 0));
		break;
	case 0x99:
		move = doQueenMove(1, OFFSET(4, 0));
		break;
	case 0x9a:
		move = doQueenMove(1, OFFSET(5, 0));
		break;
	case 0x9b:
		move = doQueenMove(1, OFFSET(6, 0));
		break;
	case 0x9c:
		move = doQueenMove(1, OFFSET(7, 0));
		break;
	case 0x9d:
		move = doQueenMove(1, OFFSET(1, 1));
		break;
	case 0x9e:
		move = doQueenMove(1, OFFSET(2, 2));
		break;
	case 0x9f:
		move = doQueenMove(1, OFFSET(3, 3));
		break;
	case 0xa0:
		move = doQueenMove(1, OFFSET(4, 4));
		break;
	case 0xa1:
		move = doQueenMove(1, OFFSET(5, 5));
		break;
	case 0xa2:
		move = doQueenMove(1, OFFSET(6, 6));
		break;
	case 0xa3:
		move = doQueenMove(1, OFFSET(7, 7));
		break;
	case 0xa4:
		move = doQueenMove(1, OFFSET(1, 7));
		break;
	case 0xa5:
		move = doQueenMove(1, OFFSET(2, 6));
		break;
	case 0xa6:
		move = doQueenMove(1, OFFSET(3, 5));
		break;
	case 0xa7:
		move = doQueenMove(1, OFFSET(4, 4));
		break;
	case 0xa8:
		move = doQueenMove(1, OFFSET(5, 3));
		break;
	case 0xa9:
		move = doQueenMove(1, OFFSET(6, 2));
		break;
	case 0xaa:
		move = doQueenMove(1, OFFSET(7, 1));
		break;

	// Third Queen ##########################
	case 0xab:
		move = doQueenMove(2, OFFSET(0, 1));
		break;
	case 0xac:
		move = doQueenMove(2, OFFSET(0, 2));
		break;
	case 0xad:
		move = doQueenMove(2, OFFSET(0, 3));
		break;
	case 0xae:
		move = doQueenMove(2, OFFSET(0, 4));
		break;
	case 0xaf:
		move = doQueenMove(2, OFFSET(0, 5));
		break;
	case 0xb0:
		move = doQueenMove(2, OFFSET(0, 6));
		break;
	case 0xb1:
		move = doQueenMove(2, OFFSET(0, 7));
		break;
	case 0xb2:
		move = doQueenMove(2, OFFSET(1, 0));
		break;
	case 0xb3:
		move = doQueenMove(2, OFFSET(2, 0));
		break;
	case 0xb4:
		move = doQueenMove(2, OFFSET(3, 0));
		break;
	case 0xb5:
		move = doQueenMove(2, OFFSET(4, 0));
		break;
	case 0xb6:
		move = doQueenMove(2, OFFSET(5, 0));
		break;
	case 0xb7:
		move = doQueenMove(2, OFFSET(6, 0));
		break;
	case 0xb8:
		move = doQueenMove(2, OFFSET(7, 0));
		break;
	case 0xb9:
		move = doQueenMove(2, OFFSET(1, 1));
		break;
	case 0xba:
		move = doQueenMove(2, OFFSET(2, 2));
		break;
	case 0xbb:
		move = doQueenMove(2, OFFSET(3, 3));
		break;
	case 0xbc:
		move = doQueenMove(2, OFFSET(4, 4));
		break;
	case 0xbd:
		move = doQueenMove(2, OFFSET(5, 5));
		break;
	case 0xbe:
		move = doQueenMove(2, OFFSET(6, 6));
		break;
	case 0xbf:
		move = doQueenMove(2, OFFSET(7, 7));
		break;
	case 0xc0:
		move = doQueenMove(2, OFFSET(1, 7));
		break;
	case 0xc1:
		move = doQueenMove(2, OFFSET(2, 6));
		break;
	case 0xc2:
		move = doQueenMove(2, OFFSET(3, 5));
		break;
	case 0xc3:
		move = doQueenMove(2, OFFSET(4, 4));
		break;
	case 0xc4:
		move = doQueenMove(2, OFFSET(5, 3));
		break;
	case 0xc5:
		move = doQueenMove(2, OFFSET(6, 2));
		break;
	case 0xc6:
		move = doQueenMove(2, OFFSET(7, 1));
		break;

	// Third Rook ##########################
	case 0xc7:
		move = doRookMove(2, OFFSET(0, 1));
		break;
	case 0xc8:
		move = doRookMove(2, OFFSET(0, 2));
		break;
	case 0xc9:
		move = doRookMove(2, OFFSET(0, 3));
		break;
	case 0xca:
		move = doRookMove(2, OFFSET(0, 4));
		break;
	case 0xcb:
		move = doRookMove(2, OFFSET(0, 5));
		break;
	case 0xcc:
		move = doRookMove(2, OFFSET(0, 6));
		break;
	case 0xcd:
		move = doRookMove(2, OFFSET(0, 7));
		break;
	case 0xce:
		move = doRookMove(2, OFFSET(1, 0));
		break;
	case 0xcf:
		move = doRookMove(2, OFFSET(2, 0));
		break;
	case 0xd0:
		move = doRookMove(2, OFFSET(3, 0));
		break;
	case 0xd1:
		move = doRookMove(2, OFFSET(4, 0));
		break;
	case 0xd2:
		move = doRookMove(2, OFFSET(5, 0));
		break;
	case 0xd3:
		move = doRookMove(2, OFFSET(6, 0));
		break;
	case 0xd4:
		move = doRookMove(2, OFFSET(7, 0));
		break;

	// Third Bishop ########################
	case 0xd5:
		move = doBishopMove(2, OFFSET(1, 1));
		break;
	case 0xd6:
		move = doBishopMove(2, OFFSET(2, 2));
		break;
	case 0xd7:
		move = doBishopMove(2, OFFSET(3, 3));
		break;
	case 0xd8:
		move = doBishopMove(2, OFFSET(4, 4));
		break;
	case 0xd9:
		move = doBishopMove(2, OFFSET(5, 5));
		break;
	case 0xda:
		move = doBishopMove(2, OFFSET(6, 6));
		break;
	case 0xdb:
		move = doBishopMove(2, OFFSET(7, 7));
		break;
	case 0xdc:
		move = doBishopMove(2, OFFSET(1, 7));
		break;
	case 0xdd:
		move = doBishopMove(2, OFFSET(2, 6));
		break;
	case 0xde:
		move = doBishopMove(2, OFFSET(3, 5));
		break;
	case 0xdf:
		move = doBishopMove(2, OFFSET(4, 4));
		break;
	case 0xe0:
		move = doBishopMove(2, OFFSET(5, 3));
		break;
	case 0xe1:
		move = doBishopMove(2, OFFSET(6, 2));
		break;
	case 0xe2:
		move = doBishopMove(2, OFFSET(7, 1));
		break;

	// Third Knight ########################
	case 0xe3:
		move = doKnightMove(2, OFFSET(+2, +1));
		break;
	case 0xe4:
		move = doKnightMove(2, OFFSET(+1, +2));
		break;
	case 0xe5:
		move = doKnightMove(2, OFFSET(-1, +2));
		break;
	case 0xe6:
		move = doKnightMove(2, OFFSET(-2, +1));
		break;
	case 0xe7:
		move = doKnightMove(2, OFFSET(-2, -1));
		break;
	case 0xe8:
		move = doKnightMove(2, OFFSET(-1, -2));
		break;
	case 0xe9:
		move = doKnightMove(2, OFFSET(+1, -2));
		break;
	case 0xea:
		move = doKnightMove(2, OFFSET(+2, -1));
		break;

	// Multiple byte move ##################
	case 0xeb: {
		// not handled yet;
	} break;

	// Padding #############################
	case 0xec:
		return Token_Skip;

	// Unused ##############################
	case 0xed: // fallthru
	case 0xee: // fallthru
	case 0xef: // fallthru
	case 0xf0: // fallthru
	case 0xf1: // fallthru
	case 0xf2: // fallthru
	case 0xf3: // fallthru
	case 0xf4: // fallthru
	case 0xf5: // fallthru
	case 0xf6: // fallthru
	case 0xf7: // fallthru
	case 0xf8: // fallthru
	case 0xf9: // fallthru
	case 0xfa: // fallthru
	case 0xfb: // fallthru
	case 0xfc: // fallthru
	case 0xfd:
		return Token_Skip;

	// Push position #######################
	case 0xfe:
		return Token_Push;

	// Pop position ########################
	case 0xff:
		return Token_Pop;

#undef OFFSET
	}

	return move;
}

class CbgParser {
	FilebufAppend* stream_;

public:
	CbgParser() = default;
	explicit CbgParser(FilebufAppend* stream) : stream_(stream) {}

	errorT parseNext(Game& game, uint32_t offset) {
		printf("Parse game with offset %d\n", offset);
		stream_->pubseekpos(offset + 4);
		unsigned int count = 0;
		unsigned int skipped = 0;
		while (count < 150) {
			char c[1];
			stream_->sgetn(c, 1);
			byte b = static_cast<byte>(c[0]);
			byte pos = byte(b - count + skipped);
			byte move = MoveNumberLookup[pos];
			if (pos == 0xeb)
				printf("Multibyte move");
			count++;
			std::string move_string = decodeMove(move);
			if (move_string == "skip" || move_string == "push" ||
			    move_string == "pop")
				skipped += 1;
			if (move_string == "pop")
				return OK;
			std::string num = std::to_string((count + 1) / 2);
			std::string start = (count % 2) == 1
			                        ? num + "."
			                        : std::string(num.length() + 1, ' ');
			printf("%s %s\n", start.c_str(), move_string.c_str());
			if (move == 0xfd)
				break;
		}
		return OK;
	}
};
