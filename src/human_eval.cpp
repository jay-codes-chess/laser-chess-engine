/*
    Human Chess Engine - Teaching Evaluation Implementation
    Based on Laser by Jeffrey An and Michael An
*/

#include "human_eval.h"
#include "board.h"
#include "eval.h"
#include <algorithm>
#include <cmath>

namespace HumanEval {

static PlayingStyle current_style = PlayingStyle::CLASSICAL;

static double classical_mults[2] = {1.0, 1.0};
static double attacking_mults[2] = {1.0, 0.8};
static double tactical_mults[2] = {1.2, 0.8};
static double positional_mults[2] = {0.8, 1.2};
static double technical_mults[2] = {0.7, 1.3};

void setStyle(PlayingStyle style) { current_style = style; }
PlayingStyle getStyle() { return current_style; }
const double* getStyleMultipliers() {
    switch (current_style) {
        case PlayingStyle::CLASSICAL:  return classical_mults;
        case PlayingStyle::ATTACKING:  return attacking_mults;
        case PlayingStyle::TACTICAL:   return tactical_mults;
        case PlayingStyle::POSITIONAL: return positional_mults;
        case PlayingStyle::TECHNICAL:  return technical_mults;
        default: return classical_mults;
    }
}

bool isPassedPawn(const Board& b, int color, int sq) {
    int file = sq % 8;
    int rank = sq / 8;
    uint64_t own_pawns = b.getPieces(color, PAWNS);
    
    for (int f = file - 1; f >= 0; f--) {
        int check_sq = f * 8 + rank;
        if (own_pawns & (1ULL << check_sq)) return false;
    }
    for (int f = file + 1; f < 8; f++) {
        int check_sq = f * 8 + rank;
        if (own_pawns & (1ULL << check_sq)) return false;
    }
    
    int capture_rank = (color == WHITE) ? rank + 1 : rank - 1;
    if (capture_rank >= 0 && capture_rank < 8) {
        uint64_t capture_mask = 0;
        if (file > 0) capture_mask |= (1ULL << (file - 1 + capture_rank * 8));
        if (file < 7) capture_mask |= (1ULL << (file + 1 + capture_rank * 8));
        if (b.getPieces(1-color, PAWNS) & capture_mask) return false;
    }
    return true;
}

ImbalanceAnalysis analyzeImbalances(const Board& b) {
    ImbalanceAnalysis ia = {};
    
    int piece_values[5] = {100, 320, 330, 500, 900};
    for (int pt = 0; pt < 5; pt++) {
        int white_count = __builtin_popcountll(b.getPieces(WHITE, pt + 1));
        int black_count = __builtin_popcountll(b.getPieces(BLACK, pt + 1));
        ia.material += (white_count - black_count) * piece_values[pt];
    }
    
    uint64_t white_pawns = b.getPieces(WHITE, PAWNS);
    uint64_t black_pawns = b.getPieces(BLACK, PAWNS);
    
    uint64_t temp = white_pawns;
    while (temp) {
        int sq = __builtin_ctzll(temp);
        if (isPassedPawn(b, WHITE, sq)) {
            ia.white_has_passed_pawn = true;
            ia.pawn_structure += 20;
        }
        temp &= temp - 1;
    }
    
    temp = black_pawns;
    while (temp) {
        int sq = __builtin_ctzll(temp);
        if (isPassedPawn(b, BLACK, sq)) {
            ia.black_has_passed_pawn = true;
            ia.pawn_structure -= 20;
        }
        temp &= temp - 1;
    }
    
    uint64_t white_piece_mask = b.getPieces(WHITE, PAWNS) | b.getPieces(WHITE, KNIGHTS) | 
                               b.getPieces(WHITE, BISHOPS) | b.getPieces(WHITE, ROOKS) |
                               b.getPieces(WHITE, QUEENS) | b.getPieces(WHITE, KINGS);
    uint64_t black_piece_mask = b.getPieces(BLACK, PAWNS) | b.getPieces(BLACK, KNIGHTS) | 
                               b.getPieces(BLACK, BISHOPS) | b.getPieces(BLACK, ROOKS) |
                               b.getPieces(BLACK, QUEENS) | b.getPieces(BLACK, KINGS);
    
    int white_space = 0, black_space = 0;
    for (int sq = 0; sq < 32; sq++) {
        if (white_piece_mask & (1ULL << sq)) white_space++;
        if (black_piece_mask & (1ULL << (sq + 32))) black_space++;
    }
    ia.space = (white_space - black_space) * 5;
    
    uint64_t white_non_pawns = b.getPieces(WHITE, KNIGHTS) | b.getPieces(WHITE, BISHOPS) |
                              b.getPieces(WHITE, ROOKS) | b.getPieces(WHITE, QUEENS);
    uint64_t black_non_pawns = b.getPieces(BLACK, KNIGHTS) | b.getPieces(BLACK, BISHOPS) |
                              b.getPieces(BLACK, ROOKS) | b.getPieces(BLACK, QUEENS);
    
    uint64_t white_back_rank = 0xFFULL << 56 | 0xFFULL;  // a8-h8 and a1-h1
    uint64_t black_back_rank = 0xFFULL;
    
    int white_dev = __builtin_popcountll(white_non_pawns & ~white_back_rank);
    int black_dev = __builtin_popcountll(black_non_pawns & ~black_back_rank);
    ia.development = (white_dev - black_dev) * 30;
    
    ia.initiative = (b.getPlayerToMove() == WHITE) ? 10 : -10;
    
    if (!(b.getCastlingRights() & WHITECASTLE)) {
        ia.white_king_exposed = (b.getKingSq(WHITE) >= 56);
        ia.king_safety -= 30;
    }
    if (!(b.getCastlingRights() & BLACKCASTLE)) {
        ia.black_king_exposed = (b.getKingSq(BLACK) <= 7);
        ia.king_safety += 30;
    }
    
    return ia;
}

MoveExplanation explainMove(const Board& b, int move, const ImbalanceAnalysis& ia) {
    MoveExplanation exp = {};
    
    if (ia.material > 200) {
        exp.imbalance_notes.push_back("Material advantage (+" + std::to_string(ia.material/100) + ")");
    } else if (ia.material < -200) {
        exp.imbalance_notes.push_back("Material deficit (" + std::to_string(ia.material/100) + ")");
    }
    
    if (ia.white_has_passed_pawn) exp.imbalance_notes.push_back("Passed pawn");
    if (ia.black_has_passed_pawn) exp.imbalance_notes.push_back("Opponent has passed pawn");
    if (ia.initiative > 15) {
        exp.imbalance_notes.push_back("Strong initiative");
        exp.move_reasons.push_back("Maintaining the initiative");
    }
    if (ia.white_king_exposed) {
        exp.imbalance_notes.push_back("King safety a concern");
        exp.move_reasons.push_back("Kingside needs attention");
    }
    if (ia.black_king_exposed) {
        exp.imbalance_notes.push_back("Opposing king is exposed");
        exp.move_reasons.push_back("Attacking the exposed king");
    }
    if (ia.space > 30) exp.move_reasons.push_back("More space for pieces");
    if (ia.development > 60) exp.move_reasons.push_back("Better development");
    
    exp.pv_explanation = "";
    for (size_t i = 0; i < exp.move_reasons.size(); i++) {
        if (i > 0) exp.pv_explanation += " | ";
        exp.pv_explanation += exp.move_reasons[i];
    }
    if (exp.pv_explanation.empty()) exp.pv_explanation = "Typical developing move";
    
    return exp;
}

int endgamePatienceBonus(const Board& b, int color) {
    int total_material = 0;
    int piece_values[5] = {100, 320, 330, 500, 900};
    for (int pt = 0; pt < 5; pt++) {
        total_material += __builtin_popcountll(b.getPieces(0, pt + 1)) * piece_values[pt];
        total_material += __builtin_popcountll(b.getPieces(1, pt + 1)) * piece_values[pt];
    }
    if (total_material > 2500) return 0;
    
    int king_sq = b.getKingSq(color);
    int opp_king_sq = b.getKingSq(1-color);
    int king_file = king_sq % 8;
    int king_rank = king_sq / 8;
    int opp_king_file = opp_king_sq % 8;
    int opp_king_rank = opp_king_sq / 8;
    
    int dist_to_center = abs(king_file - 3) + abs(king_rank - 3);
    int opp_dist_to_center = abs(opp_king_file - 3) + abs(opp_king_rank - 3);
    
    int bonus = 0;
    if (dist_to_center < opp_dist_to_center) {
        bonus += (opp_dist_to_center - dist_to_center) * 5;
    }
    
    int rank_diff = abs(king_rank - opp_king_rank);
    int file_diff = abs(king_file - opp_king_file);
    if ((rank_diff + file_diff) % 2 == 0 && (rank_diff + file_diff) > 0) {
        bonus += 15;
    }
    return bonus;
}

int initiativeBonus(const Board& b, int color) {
    int bonus = 0;
    if (b.getPlayerToMove() == color) bonus += 10;
    
    uint64_t color_pieces = b.getPieces(color, KNIGHTS) | b.getPieces(color, BISHOPS) |
                           b.getPieces(color, ROOKS) | b.getPieces(color, QUEENS);
    int active_pieces = __builtin_popcountll(color_pieces);
    bonus += active_pieces * 5;
    return bonus;
}

int prophylaxisBonus(const Board& b, int color) {
    uint64_t opp_pawns = b.getPieces(1-color, PAWNS);
    int file_a = __builtin_popcountll(opp_pawns & 0x0101010101010101ULL);
    int file_h = __builtin_popcountll(opp_pawns & 0x8080808080808080ULL);
    return (file_a == 0 || file_h == 0) ? 10 : 0;
}

} // namespace HumanEval
