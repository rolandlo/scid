/*
 * Copyright (C) 2026 Roland Lötscher
 *
 * Scid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Scid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Scid. If not, see <http://www.gnu.org/licenses/>.
 */

#include "codec_cbh.h"
#include "game.h"
#include "scidbase.h"
#include <algorithm>
#include <bytebuf.h>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <random>

namespace {

std::tuple<const char*, const char*> databases[] = {
    {SCID_TESTDIR "NormalGames/BielMTO.cbh",
     SCID_TESTDIR "NormalGames/BielMTO.pgn"},
    {SCID_TESTDIR "NonStandard/NonStandardStart.cbh",
     SCID_TESTDIR "NonStandard/NonStandardStart.pgn"},
    {SCID_TESTDIR "WithVariations/WithVariations.cbh",
     SCID_TESTDIR "WithVariations/GamesWithVariations.pgn"},
    {SCID_TESTDIR "Chess960Biel/Chess960Biel.cbh",
     SCID_TESTDIR "Chess960Biel/Chess960.pgn"},
    {SCID_TESTDIR "NormalNoPop/StandardMissingPop.cbh",
     SCID_TESTDIR "NormalNoPop/StandardNoPop.pgn"},
    {SCID_TESTDIR "ManyPromotions/ManyPromotions.cbh",
     SCID_TESTDIR "ManyPromotions/Promotions.pgn"},
    {SCID_TESTDIR "Chess960NonStandard/Chess960NonStandard.cbh",
     SCID_TESTDIR "Chess960NonStandard/Chess960NonStandardStart.pgn"},
};

const char* unusualBase = SCID_TESTDIR
    "UnusualStart/UnusualStartBytes.cbh"; // 9 games from the MegaBase 2021
                                          // which start with unusual bytes (1,
                                          // 4 or 5) and can be decoded by
                                          // Chessbase.
const char* corruptBase = SCID_TESTDIR
    "Chess960Corrupt/Corrupt.cbh"; // 4 from the MegaBase 2021 which can't be
                                   // decoded by Chessbase either (and make
                                   // Chessbase crash when trying to open in one
                                   // case)

// Database with guiding text
// Games with null moves

} // namespace

TEST(Test_CBH_Codec, compareWithPGN) {
	for (auto [cbhname, pgnname] : databases) {

		scidBaseT cbhbase;
		scidBaseT pgnbase;
		ASSERT_EQ(OK, cbhbase.open("CBH", FMODE_Both, cbhname));
		ASSERT_EQ(OK, pgnbase.open("PGN", FMODE_Both, pgnname));

		auto numGames = pgnbase.numGames();
		ASSERT_EQ(numGames, cbhbase.numGames());

		for (int i = 0; i < numGames; i++) {
			ASSERT_NE(nullptr, cbhbase.getIndexEntry_bounds(i));
			ASSERT_NE(nullptr, pgnbase.getIndexEntry_bounds(i));
			Game cbhGame, pgnGame;
			auto cbhBufGame = cbhbase.getGame(*cbhbase.getIndexEntry(i));
			auto pgnBufGame = pgnbase.getGame(*pgnbase.getIndexEntry(i));
			ASSERT_TRUE(cbhBufGame);
			ASSERT_TRUE(pgnBufGame);
			ASSERT_EQ(OK, cbhGame.DecodeMovesOnly(cbhBufGame));
			ASSERT_EQ(OK, pgnGame.DecodeMovesOnly(pgnBufGame));
			EXPECT_EQ(pgnGame.GetNumHalfMoves(), cbhGame.GetNumHalfMoves());
			EXPECT_EQ(pgnGame.GetNumVariations(), cbhGame.GetNumVariations());

			cbhGame.MoveToStart();
			pgnGame.MoveToStart();
			char cbhStart[1024];
			cbhGame.currentPos()->PrintFEN(cbhStart);
			char pgnStart[1024];
			pgnGame.currentPos()->PrintFEN(pgnStart);
			EXPECT_STREQ(pgnStart, cbhStart);

			cbhGame.MoveToEnd();
			pgnGame.MoveToEnd();
			char cbhEnd[1024];
			cbhGame.currentPos()->PrintFEN(cbhEnd);
			char pgnEnd[1024];
			pgnGame.currentPos()->PrintFEN(pgnEnd);
			EXPECT_STREQ(pgnEnd, cbhEnd);
			EXPECT_STREQ(pgnGame.currentPosUCI().c_str(),
			             cbhGame.currentPosUCI().c_str());
		}
	}
}

TEST(Test_CBH_Codec, doNotCrashWithUnusualStartByte) {
	scidBaseT base;
	ASSERT_EQ(OK, base.open("CBH", FMODE_Both, unusualBase));
	ASSERT_EQ(9, base.numGames());

	for (int i = 0; i < base.numGames(); i++) {
		ASSERT_NE(nullptr, base.getIndexEntry_bounds(i));
		Game game;
		auto bufGame = base.getGame(*base.getIndexEntry(i));
		ASSERT_TRUE(bufGame);
		ASSERT_EQ(OK, game.DecodeMovesOnly(bufGame));
	}
}

TEST(Test_CBH_Codec, doNotCrashWithCorruptChess960Encoding) {
	scidBaseT base;
	ASSERT_EQ(OK, base.open("CBH", FMODE_Both, corruptBase));
	ASSERT_EQ(4, base.numGames());

	for (int i = 0; i < base.numGames(); i++) {
		ASSERT_NE(nullptr, base.getIndexEntry_bounds(i));
		Game game;
		auto bufGame = base.getGame(*base.getIndexEntry(i));
		ASSERT_TRUE(bufGame);
		game.DecodeMovesOnly(bufGame);
	}
}
