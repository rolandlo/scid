/*
 * Copyright (C) 2026  Roland Lötscher.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/** @file
 * Implements the CodecCBH class.
 */

#pragma once

#include "cbgparse.h"
#include "codec_proxy.h"
#include "filebuf.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

/* -----------------------------------------------------------------------------
See
https://talkchess.com/viewtopic.php?p=287896&sid=7237cb8fc0656ad4837fc9ab29cf802f#p287896
for a reference on the Chessbase .cbh database format
*/

// This class manages databases encoded in Chessbase's cbh format.
class CodecCBH final : public CodecProxy<CodecCBH> {
	FilebufAppend gfile_; // game data
	FilebufAppend pfile_; // player data
	Filebuf idxfile_;     // header file

	std::vector<std::string> filenames_;

	size_t n_games_ = 0;
	size_t n_parsed_ = 0;
	size_t player_header_size_ = 0;

	CbgParser game_parser_;

	static constexpr auto INDEX_HEADER_SIZE = 46;
	static constexpr auto INDEX_ENTRY_SIZE = 46;
	static constexpr auto PLAYER_HEADER_FIXED_SIZE = 28; // without extra
	static constexpr auto PLAYER_ENTRY_SIZE = 67;

public:
	Codec getType() const final { return ICodecDatabase::CBH; }

	std::vector<std::string> getFilenames() const final { return filenames_; };

	/**
	 * Writes all pending output to the files.
	 * @returns OK if successful or an error code.
	 */
	errorT flush() final {

		errorT errGfile = (gfile_.pubsync() == 0) ? OK : ERROR_FileWrite;
		errorT errPfile = (pfile_.pubsync() == 0) ? OK : ERROR_FileWrite;
		errorT errIndex = (idxfile_.pubsync() == 0) ? OK : ERROR_FileWrite;
		errorT errProxy = CodecProxy<CodecCBH>::flush();
		return errIndex   ? errIndex
		       : errGfile ? errGfile
		       : errPfile ? errPfile
		                  : errProxy;
	}

	/**
	 * Opens/creates a CBH database.
	 * After successfully opening/creating the file, the object is ready for
	 * parseNext() calls.
	 * @param filename: full path of the cbh file to be opened.
	 * @param fmode:    valid file access mode.
	 * @returns OK in case of success, an @e errorT code otherwise.
	 */
	errorT open(const char* filename, fileModeT fmode) {
		ASSERT(filename);

		auto dbname = std::string_view(filename);
		if (dbname.ends_with(".cbh"))
			dbname.remove_suffix(4);

		if (dbname.empty())
			return ERROR_FileOpen;

		filenames_.resize(3);
		filenames_[0].assign(dbname).append(".cbh"); // header
		filenames_[1].assign(dbname).append(".cbp"); // player data
		filenames_[2].assign(dbname).append(".cbg"); // game data

		if (fmode == FMODE_Create) {
			for (auto const& fname : filenames_) {
				std::error_code ec;
				if (std::filesystem::exists(fname, ec) || ec)
					return ERROR_Exists;
			}

			if (auto err = idxfile_.Open(filenames_[0].c_str(), fmode))
				return err;

			if (auto err = pfile_.open(filenames_[1], fmode))
				return err;

			if (auto err = gfile_.open(filenames_[2], fmode))
				return err;

			return OK;
		}

		auto err_idx = read_index_header(fmode, filenames_[0].c_str());
		auto err_pl = read_player_header(fmode, filenames_[1].c_str());
		auto err_gm = read_game_header(fmode, filenames_[2].c_str());

		game_parser_ = CbgParser(&gfile_);

		return err_idx ? err_idx : err_pl ? err_pl : err_gm;
	}

	/**
	 * Reads the next game.
	 * @param game: the Game object where the data will be stored.
	 * @returns
	 * - ERROR_NotFound if there are no more games to be read.
	 * - OK otherwise.
	 */
	errorT parseNext(Game& game) {
		if (n_parsed_ >= n_games_)
			return ERROR_NotFound;

		byte flags = idxfile_.ReadOneByte();
		uint32_t game_offset = idxfile_.ReadFourBytes();
		uint32_t annotation_offset = idxfile_.ReadFourBytes();
		uint white_player = idxfile_.ReadThreeBytes();
		uint black_player = idxfile_.ReadThreeBytes();
		uint tournament = idxfile_.ReadThreeBytes();
		uint annotator = idxfile_.ReadThreeBytes();
		uint source = idxfile_.ReadThreeBytes();
		uint date = idxfile_.ReadThreeBytes();
		byte res = idxfile_.ReadOneByte();
		byte line_eval = idxfile_.ReadOneByte();
		byte round = idxfile_.ReadOneByte();
		byte subround = idxfile_.ReadOneByte();
		uint16_t white_rating = idxfile_.ReadTwoBytes();
		uint16_t black_rating = idxfile_.ReadTwoBytes();
		idxfile_.pubseekoff(INDEX_ENTRY_SIZE - 35, std::ios::cur, std::ios::in);

		uint year = (date >> 9) & 4095;
		uint month = (date >> 5) & 15;
		uint day = date & 31;

		std::string round_string = std::to_string(round) + "." +
		                           std::to_string(subround);

		resultT result = res == 2   ? RESULT_White
		                 : res == 1 ? RESULT_Draw
		                 : res == 0 ? RESULT_Black
		                            : RESULT_None;

		pfile_.pubseekpos(player_header_size_ +
		                  white_player * PLAYER_ENTRY_SIZE +
		                  9); // move to offset 9
		char white_last_name[31] = {0};
		char white_first_name[21] = {0};
		pfile_.sgetn(white_last_name, 30);
		pfile_.sgetn(white_first_name, 20);
		std::string white_string = std::string(white_last_name) + ", " +
		                           std::string(white_first_name);

		pfile_.pubseekpos(player_header_size_ +
		                  black_player * PLAYER_ENTRY_SIZE +
		                  9); // move to offset 9
		char black_last_name[31] = {0};
		char black_first_name[21] = {0};
		pfile_.sgetn(black_last_name, 30);
		pfile_.sgetn(black_first_name, 20);
		std::string black_string = std::string(black_last_name) + ", " +
		                           std::string(black_first_name);

		game.Clear();
		game.SetWhiteStr(white_string.c_str());
		game.SetBlackStr(black_string.c_str());
		game.SetDate(DATE_MAKE(year, month, day));
		game.SetWhiteElo(white_rating & 0xFFF);
		game.SetBlackElo(black_rating & 0xFFF);
		game.SetRoundStr(round_string.c_str());
		game.SetResult(result);
		errorT err_game = game_parser_.parseNext(game, game_offset);

		n_parsed_ += 1;

		return err_game;
	}

	/**
	 * Returns info about the parsing progress.
	 * @returns a pair<size_t, size_t> where first element is the quantity of
	 * data parsed and second one is the total amount of data of the database.
	 */
	std::pair<size_t, size_t> parseProgress() {
		return std::make_pair(n_parsed_, n_games_);
	}

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
	errorT read_index_header(fileModeT fmode, const char* fname) {
		if (auto err = idxfile_.Open(fname, fmode))
			return err;

		const auto file_size = idxfile_.pubseekoff(0, std::ios::end);
		int entries_size = static_cast<int>(file_size) - INDEX_HEADER_SIZE;
		if (entries_size < 0 || (entries_size % INDEX_ENTRY_SIZE) != 0 ||
		    idxfile_.pubseekoff(0, std::ios::beg) != 0)
			return ERROR_Corrupt;

		n_games_ = entries_size / INDEX_ENTRY_SIZE;

		uint head1 = idxfile_.ReadThreeBytes();
		uint head2 = idxfile_.ReadThreeBytes();
		if ((head1 != 0x00002C && head1 != 0x000024) || head2 != 0x002E01)
			return ERROR_BadMagic;

		const std::streamsize remaining = INDEX_HEADER_SIZE - 6;
		char dummy[remaining];
		idxfile_.sgetn(dummy, remaining);

		return OK;
	}

	errorT read_player_header(fileModeT fmode, const char* fname) {
		if (auto err = pfile_.open(fname, fmode))
			return err;

		pfile_.pubseekpos(PLAYER_HEADER_FIXED_SIZE - 4);

		char extra[1];
		pfile_.sgetn(extra, 1);
		player_header_size_ = PLAYER_HEADER_FIXED_SIZE +
		                      static_cast<byte>(extra[0]);

		return OK;
	}

	errorT read_game_header(fileModeT fmode, const char* fname) {
		if (auto err = gfile_.open(fname, fmode))
			return err;

		return OK;
	}
};
