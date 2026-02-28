/*
 * Copyright (C) 2026  Roland Lötscher

 * This file is part of Scid (Shane's Chess Information Database).
 *
 * Scid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Scid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Scid.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/** @file
 * Implements the CodecCBH class that manages databases encoded in CBH
 * format.
 */

#include "codec_cbh.h"
#include "libcbh/interface.h"

const std::string squares[64] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", // 1st row
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", // 2nd row
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", // 3rd row
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", // 4th row
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", // 5th row
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", // 6th row
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", // 7th row
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"  // 8th row
};

static inline std::string squareName(byte sq) {
	if (0 <= sq && sq < 64) {
		return squares[sq];
	}
	return "";
}

errorT CodecCBH::open(const char* filename, fileModeT fmode) {
	ASSERT(filename);

	if (auto err = codec_.open(filename); err)
		return err;

	n_games_ = codec_.numGames();

	auto dbname = std::string_view(filename);
	if (dbname.ends_with(".cbh"))
		dbname.remove_suffix(4);

	if (dbname.empty())
		return ERROR_FileOpen;

	return OK;
}

void CodecCBH::addAnnotations(std::vector<Comment>& comments, Game& game) {
	for (const auto& item : comments) {
		std::visit(
		    [&](const auto& comment) {
			    using T = std::decay_t<decltype(comment)>;
			    if constexpr (std::is_same_v<T, TextAfterComment>) {
				    auto& str = game.accessMoveComment();
				    str = comment.text + str;
			    } else if constexpr (std::is_same_v<T, TextBeforeComment>) {
				    // Append comment to previous move
				    game.MoveBackup();
				    auto& str = game.accessMoveComment();
				    str = str + comment.text;
				    game.MoveForward();
			    } else if constexpr (std::is_same_v<T, ArrowComment>) {
				    std::string from = squareName(comment.from);
				    std::string to = squareName(comment.to);

				    std::string annotation = "[%draw arrow," + from + "," + to +
				                             "," + comment.color + "]";
				    auto& str = game.accessMoveComment();
				    str = str + annotation;
			    } else if constexpr (std::is_same_v<T, SquareComment>) {
				    std::string square = squareName(comment.sq);
				    std::string annotation = "[%draw full," + square + "," +
				                             comment.color + "]";
				    auto& str = game.accessMoveComment();
				    str = str + annotation;
			    } else if constexpr (std::is_same_v<T, SymbolComment>) {
				    if (comment.symbol)
					    game.AddNag(comment.symbol);
				    if (comment.evaluation)
					    game.AddNag(comment.evaluation);
				    if (comment.prefix)
					    game.AddNag(comment.prefix);
			    }
		    },
		    item);
	}
}

uint32_t CodecCBH::addAnnotatedMoves(std::vector<AnnotatedMove>& moves,
                                     Game& game, uint32_t start) {
	uint32_t i = start;
	while (i < moves.size()) {
		auto m = moves[i++];
		simpleMoveT sm;
		sm.from = m.from;
		sm.to = m.to;
		sm.promote = m.promote;
		sm.castling = 0;
		switch (m.promote) {
		case byte(-1): { // Push
			auto location = game.currentLocation();
			i = addAnnotatedMoves(moves, game, i);
			game.restoreLocation(location);
			game.MoveForward();
			game.AddVariation();
			continue;
		};
		case byte(-2): { // Pop
			return i;
		}
		case byte(-3): { // Skip
			continue;
		}
		case PAWN: { // null move
			sm.movingPiece = KING;
			break;
		}
		case KING: { // castling
			sm.promote = EMPTY;
			sm.castling = 1;
			break;
		}
		}
		game.GetCurrentPos()->fillMove(sm);
		game.AddMove(sm);
		if (!m.comments.empty()) {
			addAnnotations(m.comments, game);
		}
	}
	return i;
}

errorT CodecCBH::parseNext(Game& game) {
	if (n_parsed_ >= n_games_)
		return ERROR_NotFound;

	GameReturnValue ret;
	codec_.parseNext(ret);
	game.Clear();
	game.SetWhiteStr((ret.whiteName + ", " + ret.whiteFirstName).c_str());
	game.SetBlackStr((ret.blackName + ", " + ret.blackFirstName).c_str());
	game.SetDate(
	    DATE_MAKE(ret.gameDate.year, ret.gameDate.month, ret.gameDate.day));
	game.SetWhiteElo(ret.whiteElo);
	game.SetBlackElo(ret.blackElo);
	game.SetRoundStr(
	    (std::to_string(ret.round) + "." + std::to_string(ret.subround))
	        .c_str());
	game.SetResult(ret.result);
	game.SetEventStr(ret.eventTitle.c_str());
	game.SetSiteStr(ret.eventPlace.c_str());
	game.SetEventDate(
	    DATE_MAKE(ret.eventDate.year, ret.eventDate.month, ret.eventDate.day));

	for (const auto& t : ret.tags)
		game.addTag(t.tag, t.value);

	game.SetStartFen(ret.startFen.c_str());
	addAnnotatedMoves(ret.annotatedMoves, game);

	n_parsed_ += 1;

	return OK;
}

std::pair<size_t, size_t> CodecCBH::parseProgress() {
	return std::make_pair(n_parsed_, n_games_);
}
