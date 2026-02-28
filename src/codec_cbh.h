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

#pragma once

#include "codec_proxy.h"
#include "libcbh/cbh.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

// This class manages databases encoded in Chessbase's cbh format.
class CodecCBH final : public CodecProxy<CodecCBH> {

	size_t n_games_ = 0;
	size_t n_parsed_ = 0;

	CbhCodec codec_;

public:
	Codec getType() const final { return ICodecDatabase::CBH; }

	std::vector<std::string> getFilenames() const final {
		return codec_.getFilenames();
	};

	/**
	 * Writes all pending output to the files.
	 * @returns OK if successful or an error code.
	 */
	errorT flush() final { return ERROR_CodecUnsupFeat; }

	/**
	 * Opens/creates a CBH database.
	 * After successfully opening/creating the file, the object is ready for
	 * parseNext() calls.
	 * @param filename: full path of the cbh file to be opened.
	 * @param fmode:    valid file access mode.
	 * @returns OK in case of success, an @e errorT code otherwise.
	 */
	errorT open(const char* filename, fileModeT fmode);

	/**
	 * Reads the next game.
	 * @param game: the Game object where the data will be stored.
	 * @returns
	 * - ERROR_NotFound if there are no more games to be read.
	 * - OK otherwise.
	 */
	errorT parseNext(Game& game);

	/**
	 * Returns info about the parsing progress.
	 * @returns a pair<size_t, size_t> where first element is the quantity of
	 * data parsed and second one is the total amount of data of the database.
	 */
	std::pair<size_t, size_t> parseProgress();

	/**
	 * Returns the list of errors produced by parseNext() calls.
	 */
	const char* parseErrors() { return NULL; }

	/**
	 * Add a game into the database.
	 * The @e game is encoded in cbh format and appended.
	 * @param game: valid pointer to a Game object with the new data.
	 * @returns OK in case of success, an @e errorT code otherwise.
	 */
	errorT gameAdd(Game* game) { return ERROR_CodecUnsupFeat; }

private:
	void addAnnotations(std::vector<Comment>& comments, Game& game);

	uint32_t addAnnotatedMoves(std::vector<AnnotatedMove>& moves, Game& game,
	                           uint32_t start = 0);
};
