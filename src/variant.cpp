/*
  Fairy-Stockfish, a UCI chess variant playing engine derived from Stockfish
  Copyright (C) 2018-2022 Fabian Fichter

  Fairy-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Fairy-Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include "parser.h"
#include "piece.h"
#include "variant.h"

using std::string;

namespace Stockfish {

VariantMap variants; // Global object

namespace {
    // Base variant
    Variant* variant_base() {
        Variant* v = new Variant();
        return v;
    }
    // Base for all fairy variants
    Variant* chess_variant_base() {
        Variant* v = variant_base()->init();
        v->pieceToCharTable = "PNBRQ................Kpnbrq................k";
        return v;
    }
    // Standard chess
    // https://en.wikipedia.org/wiki/Chess
    Variant* chess_variant() {
        Variant* v = chess_variant_base()->init();
        v->nnueAlias = "nn-";
        return v;
    }
    // Pseudo-variant only used for endgame initialization
    Variant* fairy_variant() {
        Variant* v = chess_variant_base()->init();
        v->add_piece(SILVER, 's');
        v->add_piece(FERS, 'f');
        return v;
    }
    // Ataxx
    // https://en.wikipedia.org/wiki/Ataxx
    Variant* ataxx_variant() {
        Variant* v = chess_variant_base()->init();
        v->pieceToCharTable = "P.................p.................";
        v->maxRank = RANK_7;
        v->maxFile = FILE_G;
        v->reset_pieces();
        v->add_piece(CUSTOM_PIECE_1, 'p', "mDmNmA");
        v->startFen = "P5p/7/7/7/7/7/p5P w 0 1";
        v->pieceDrops = true;
        v->doubleStep = false;
        v->castling = false;
        v->immobilityIllegal = false;
        v->stalemateValue = -VALUE_MATE;
        v->stalematePieceCount = true;
        v->passOnStalemate = true;
        v->enclosingDrop = ATAXX;
        v->flipEnclosedPieces = ATAXX;
        v->materialCounting = UNWEIGHTED_MATERIAL;
        v->nMoveRule = 0;
        v->freeDrops = true;
        return v;
    }
    // Flipersi
    // https://en.wikipedia.org/wiki/Reversi
    Variant* flipersi_variant() {
        Variant* v = chess_variant_base()->init();
        v->pieceToCharTable = "P.................p.................";
        v->maxRank = RANK_8;
        v->maxFile = FILE_H;
        v->reset_pieces();
        v->add_piece(IMMOBILE_PIECE, 'p');
        v->startFen = "8/8/8/8/8/8/8/8[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPpppppppppppppppppppppppppppppppp] w 0 1";
        v->pieceDrops = true;
        v->doubleStep = false;
        v->castling = false;
        v->immobilityIllegal = false;
        v->stalemateValue = -VALUE_MATE;
        v->stalematePieceCount = true;
        v->passOnStalemate = false;
        v->enclosingDrop = REVERSI;
        v->enclosingDropStart = make_bitboard(SQ_D4, SQ_E4, SQ_D5, SQ_E5);
        v->flipEnclosedPieces = REVERSI;
        v->materialCounting = UNWEIGHTED_MATERIAL;
        return v;
    }
    // Flipello
    // https://en.wikipedia.org/wiki/Reversi#Othello
    Variant* flipello_variant() {
        Variant* v = flipersi_variant()->init();
        v->startFen = "8/8/8/3pP3/3Pp3/8/8/8[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPpppppppppppppppppppppppppppppppp] w 0 1";
        v->passOnStalemate = true;
        return v;
    }
#ifdef LARGEBOARDS
    // Flipello 10x10
    // Othello on a 10x10 board, mainly played by computers
    // https://en.wikipedia.org/wiki/Reversi
    Variant* flipello10_variant() {
        Variant* v = flipello_variant()->init();
        v->maxRank = RANK_10;
        v->maxFile = FILE_J;
        v->startFen = "10/10/10/10/4pP4/4Pp4/10/10/10/10[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPpppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp] w - - 0 1";
        v->enclosingDropStart = make_bitboard(SQ_E5, SQ_F5, SQ_E6, SQ_F6);
        return v;
    }
#endif
} // namespace


/// VariantMap::init() is called at startup to initialize all predefined variants

void VariantMap::init() {
    // Add to UCI_Variant option
    add("chess", chess_variant());
    add("normal", chess_variant());
    add("fairy", fairy_variant()); // fairy variant used for endgame code initialization
    add("ataxx", ataxx_variant());
    add("flipersi", flipersi_variant());
    add("flipello", flipello_variant());
    #ifdef LARGEBOARDS
    add("flipello10", flipello10_variant());
#endif
}


// Pre-calculate derived properties
Variant* Variant::conclude() {
    // Enforce consistency to allow runtime optimizations
    if (!doubleStep)
        doubleStepRegion[WHITE] = doubleStepRegion[BLACK] = 0;
    if (!doubleStepRegion[WHITE] && !doubleStepRegion[BLACK])
        doubleStep = false;

    // Determine optimizations
    bool restrictedMobility = false;
    for (PieceSet ps = pieceTypes; !restrictedMobility && ps;)
    {
        PieceType pt = pop_lsb(ps);
        if (mobilityRegion[WHITE][pt] || mobilityRegion[BLACK][pt])
          restrictedMobility = true;
    }
    fastAttacks =  !(pieceTypes & ~(CHESS_PIECES | COMMON_FAIRY_PIECES))
                  && kingType == KING
                  && !restrictedMobility
                  && !cambodianMoves
                  && !diagonalLines;
    fastAttacks2 =  !(pieceTypes & ~(SHOGI_PIECES | COMMON_STEP_PIECES))
                  && kingType == KING
                  && !restrictedMobility
                  && !cambodianMoves
                  && !diagonalLines;

    // Initialize calculated NNUE properties
    nnueKing =  pieceTypes & KING ? KING
              : extinctionPieceCount == 0 && (extinctionPieceTypes & COMMONER) ? COMMONER
              : NO_PIECE_TYPE;
    // The nnueKing has to present exactly once and must not change in count
    if (nnueKing != NO_PIECE_TYPE)
    {
        // If the nnueKing is involved in promotion, count might change
        if (   ((promotionPawnTypes[WHITE] | promotionPawnTypes[BLACK]) & nnueKing)
            || ((promotionPieceTypes[WHITE] | promotionPieceTypes[BLACK]) & nnueKing)
            || std::find(std::begin(promotedPieceType), std::end(promotedPieceType), nnueKing) != std::end(promotedPieceType))
            nnueKing = NO_PIECE_TYPE;
    }
    if (nnueKing != NO_PIECE_TYPE)
    {
        std::string fenBoard = startFen.substr(0, startFen.find(' '));
        // Switch NNUE from KA to A if there is no unique piece
        if (   std::count(fenBoard.begin(), fenBoard.end(), pieceToChar[make_piece(WHITE, nnueKing)]) != 1
            || std::count(fenBoard.begin(), fenBoard.end(), pieceToChar[make_piece(BLACK, nnueKing)]) != 1)
            nnueKing = NO_PIECE_TYPE;
    }
    // We can not use popcount here yet, as the lookup tables are initialized after the variants
    int nnueSquares = (maxRank + 1) * (maxFile + 1);
    nnueUsePockets = (pieceDrops && (capturesToHand || (!mustDrop && std::bitset<64>(pieceTypes).count() != 1))) || seirawanGating;
    int nnuePockets = nnueUsePockets ? 2 * int(maxFile + 1) : 0;
    int nnueNonDropPieceIndices = (2 * std::bitset<64>(pieceTypes).count() - (nnueKing != NO_PIECE_TYPE)) * nnueSquares;
    int nnuePieceIndices = nnueNonDropPieceIndices + 2 * (std::bitset<64>(pieceTypes).count() - (nnueKing != NO_PIECE_TYPE)) * nnuePockets;
    int i = 0;
    for (PieceSet ps = pieceTypes; ps;)
    {
        // Make sure that the nnueKing type gets the last index, since the NNUE architecture relies on that
        PieceType pt = lsb(ps != piece_set(nnueKing) ? ps & ~piece_set(nnueKing) : ps);
        ps ^= pt;
        assert(pt != nnueKing || !ps);

        for (Color c : { WHITE, BLACK})
        {
            pieceSquareIndex[c][make_piece(c, pt)] = 2 * i * nnueSquares;
            pieceSquareIndex[c][make_piece(~c, pt)] = (2 * i + (pt != nnueKing)) * nnueSquares;
            pieceHandIndex[c][make_piece(c, pt)] = 2 * i * nnuePockets + nnueNonDropPieceIndices;
            pieceHandIndex[c][make_piece(~c, pt)] = (2 * i + 1) * nnuePockets + nnueNonDropPieceIndices;
        }
        i++;
    }

    // Map king squares to enumeration of actually available squares.
    // E.g., for xiangqi map from 0-89 to 0-8.
    // Variants might be initialized before bitboards, so do not rely on precomputed bitboards (like SquareBB).
    // Furthermore conclude() might be called on invalid configuration during validation,
    // therefore skip proper initialization in case of invalid board size.
    int nnueKingSquare = 0;
    if (nnueKing && nnueSquares <= SQUARE_NB)
        for (Square s = SQ_A1; s < nnueSquares; ++s)
        {
            Square bitboardSquare = Square(s + s / (maxFile + 1) * (FILE_MAX - maxFile));
            if (   !mobilityRegion[WHITE][nnueKing] || !mobilityRegion[BLACK][nnueKing]
                || (mobilityRegion[WHITE][nnueKing] & make_bitboard(bitboardSquare))
                || (mobilityRegion[BLACK][nnueKing] & make_bitboard(relative_square(BLACK, bitboardSquare, maxRank))))
            {
                kingSquareIndex[s] = nnueKingSquare++ * nnuePieceIndices;
            }
        }
    else
        kingSquareIndex[SQ_A1] = nnueKingSquare++ * nnuePieceIndices;
    nnueDimensions = nnueKingSquare * nnuePieceIndices;

    // Determine maximum piece count
    std::istringstream ss(startFen);
    ss >> std::noskipws;
    unsigned char token;
    nnueMaxPieces = 0;
    while ((ss >> token) && !isspace(token))
    {
        if (pieceToChar.find(token) != std::string::npos || pieceToCharSynonyms.find(token) != std::string::npos)
            nnueMaxPieces++;
    }
    if (twoBoards)
        nnueMaxPieces *= 2;

    // For endgame evaluation to be applicable, no special win rules must apply.
    // Furthermore, rules significantly changing game mechanics also invalidate it.
    endgameEval = extinctionValue == VALUE_NONE
                  && checkmateValue == -VALUE_MATE
                  && stalemateValue == VALUE_DRAW
                  && !materialCounting
                  && !(flagRegion[WHITE] || flagRegion[BLACK])
                  && !mustCapture
                  && !checkCounting
                  && !makpongRule
                  && !connectN
                  && !blastOnCapture
                  && !petrifyOnCaptureTypes
                  && !capturesToHand
                  && !twoBoards
                  && !restrictedMobility
                  && kingType == KING;

    shogiStylePromotions = false;
    for (PieceType current: promotedPieceType)
        if (current != NO_PIECE_TYPE)
        {
            shogiStylePromotions = true;
            break;
        }

    connect_directions.clear();
    if (connectHorizontal)
    {
        connect_directions.push_back(EAST);
    }
    if (connectVertical)
    {
        connect_directions.push_back(NORTH);
    }
    if (connectDiagonal)
    {
        connect_directions.push_back(NORTH_EAST);
        connect_directions.push_back(SOUTH_EAST);
    }

    return this;
}


/// VariantMap::parse_istream reads variants from an INI-style configuration input stream.

template <bool DoCheck>
void VariantMap::parse_istream(std::istream& file) {
    std::string variant, variant_template, key, value, input;
    while (file.peek() != '[' && std::getline(file, input)) {}

    std::vector<std::string> varsToErase = {};
    while (file.get() && std::getline(std::getline(file, variant, ']'), input))
    {
        // Extract variant template, if specified
        if (!std::getline(std::getline(std::stringstream(variant), variant, ':'), variant_template))
            variant_template = "";

        // Read variant rules
        Config attribs = {};
        while (file.peek() != '[' && std::getline(file, input))
        {
            if (!input.empty() && input.back() == '\r')
                input.pop_back();
            std::stringstream ss(input);
            if (ss.peek() != ';' && ss.peek() != '#')
            {
                if (DoCheck && !input.empty() && input.find('=') == std::string::npos)
                    std::cerr << "Invalid syntax: '" << input << "'." << std::endl;
                if (std::getline(std::getline(ss, key, '=') >> std::ws, value) && !key.empty())
                    attribs[key.erase(key.find_last_not_of(" ") + 1)] = value;
            }
        }

        // Create variant
        if (variants.find(variant) != variants.end())
            std::cerr << "Variant '" << variant << "' already exists." << std::endl;
        else if (!variant_template.empty() && variants.find(variant_template) == variants.end())
            std::cerr << "Variant template '" << variant_template << "' does not exist." << std::endl;
        else
        {
            if (DoCheck)
                std::cerr << "Parsing variant: " << variant << std::endl;
            Variant* v = !variant_template.empty() ? VariantParser<DoCheck>(attribs).parse((new Variant(*variants.find(variant_template)->second))->init())
                                                   : VariantParser<DoCheck>(attribs).parse();
            if (v->maxFile <= FILE_MAX && v->maxRank <= RANK_MAX)
            {
                add(variant, v);
                // In order to allow inheritance, we need to temporarily add configured variants
                // even when only checking them, but we remove them later after parsing is finished.
                if (DoCheck)
                    varsToErase.push_back(variant);
            }
            else
                delete v;
        }
    }
    // Clean up temporary variants
    for (std::string tempVar : varsToErase)
    {
        delete variants[tempVar];
        variants.erase(tempVar);
    }
}

/// VariantMap::parse reads variants from an INI-style configuration file.

template <bool DoCheck>
void VariantMap::parse(std::string path) {
    if (path.empty() || path == "<empty>")
        return;
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Unable to open file " << path << std::endl;
        return;
    }
    parse_istream<DoCheck>(file);
    file.close();
}

template void VariantMap::parse<true>(std::string path);
template void VariantMap::parse<false>(std::string path);

void VariantMap::add(std::string s, Variant* v) {
  insert(std::pair<std::string, const Variant*>(s, v->conclude()));
}

void VariantMap::clear_all() {
  for (auto const& element : *this)
      delete element.second;
  clear();
}

std::vector<std::string> VariantMap::get_keys() {
  std::vector<std::string> keys;
  for (auto const& element : *this)
      keys.push_back(element.first);
  return keys;
}

} // namespace Stockfish
