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

#include "cbh_decode_annotation.h"

constexpr int ANNOTATION_HEADER_SIZE = 26;
constexpr int ANNOTATION_ENTRY_SIZE = 62;

CbhAnnotationDecoder::CbhAnnotationDecoder(const char* filename,
                                           fileModeT fmode)
    : CbhDecoder(filename, fmode) {}

errorT CbhAnnotationDecoder::decode_header() {
	if (auto err = stream_.open(filename_, fmode_))
		return err;

	return OK;
}

errorT CbhAnnotationDecoder::decode_record(Game& game,
                                           std::vector<uint32_t> offsets) {
	uint32_t annotation_offset = offsets.at(0);
	stream_.pubseekpos(annotation_offset + 10); // move to offset 10

	// Read number of bytes for annotations in this game (4 bytes)
	char ls[4];
	stream_.sgetn(ls, 4);
	uint32_t length = static_cast<byte>(ls[0]) << 24 |
	                  static_cast<byte>(ls[1]) << 16 |
	                  static_cast<byte>(ls[2]) << 8 | static_cast<byte>(ls[3]);

	unsigned int readBytes = 14;
	game.MoveToStart();
	uint32_t ply = 0;
	bool finished = false;

	while (readBytes < length) {
		// Read position in game (3 bytes) big endian (mistake on talkchess?)
		char ps[3];
		stream_.sgetn(ps, 3);
		uint32_t pos = static_cast<byte>(ps[0]) << 16 |
		               static_cast<byte>(ps[1]) << 8 | static_cast<byte>(ps[2]);

		// Read type of annotation (1 byte)
		byte type = static_cast<byte>(stream_.sbumpc());

		// Read length of annotation (2 bytes, little endian) including 6
		// previous bytes
		char al[2];
		stream_.sgetn(al, 2);
		uint16_t size =
		    (static_cast<byte>(al[0]) << 8 | static_cast<byte>(al[1])) - 6;

		bool forward = true;
		while (ply < pos + 1) {
			if (forward) {
				if (!game.AtVarEnd()) {
					game.MoveForward();
					ply++;
				} else {
					forward = false;
				}
			} else {
				if (game.MoveIntoVariation(0) == OK) {
					forward = true;
				} else {
					if (!game.AtVarStart() || game.MoveExitVariation() == OK) {
						while (game.MoveBackup() != OK) {
							game.MoveExitVariation();
						}
					} else {
						finished = true;
						break;
					}
				}
			}
		}

		if (finished)
			return OK;

		char content[size + 1];
		stream_.sgetn(content, size);
		content[size] = 0;
		readBytes += size + 6;

		switch (type) {
		case 0x02: // text after move
		{
			const char* text = content + 2;
			game.SetMoveComment(text);
			break;
		}
		case 0x82: // text before move
		{
			const char* text = content + 2;
			// Append comment to previous move
			game.MoveBackup();
			auto& str = game.accessMoveComment();
			str = str + text;
			game.MoveForward();
		}
		case 0x03: // symbol
		{
			byte symb = static_cast<byte>(content[0]);
			byte moveEval = static_cast<byte>(content[1]);
			byte prefix = static_cast<byte>(content[2]);
			printf("Symbol: %d, eval: %d, prefix: %d\n", symb, moveEval,
			       prefix);
			break;
		}
		default:
			printf("Ignore annotation of type %d\n", type);
			break;
		}
	}

	return OK;
}
