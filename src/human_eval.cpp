/*
    Human Chess Engine - Teaching Evaluation Implementation
    Based on Laser by Jeffrey An and Michael An
    
    Adds Silman-style imbalances, sacrifice assessments, and teaching explanations.
*/

#include "human_eval.h"
#include "board.h"
#include "eval.h"
#include <algorithm>
#include <cmath>
#include <sstream>

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

// Count pawns of each type for structure assessment
int countPawns(const Board& b, int color) {
    uint64_t pawns = b.getPieces(color, PAWNS);
    int count = 0;
    while (pawns) {
        count++;
        pawns &= pawns - 1;
    }
    return count;
}

// Check if pawn is passed
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

// Check if pawn is isolated (no friendly pawns on adjacent files)
bool isIsolatedPawn(const Board& b, int color, int sq) {
    int file = sq % 8;
    int rank = sq / 8;
    uint64_t own_pawns = b.getPieces(color, PAWNS);
    
    // Check adjacent files
    if (file > 0) {
        uint64_t left_file = 0;
        for (int r = 0; r < 8; r++) left_file |= (1ULL << (file - 1 + r * 8));
        if (own_pawns & left_file) return false;
    }
    if (file < 7) {
        uint64_t right_file = 0;
        for (int r = 0; r < 8; r++) right_file |= (1ULL << (file + 1 + r * 8));
        if (own_pawns & right_file) return false;
    }
    return true;
}

// Analyze Silman-style imbalances
ImbalanceAnalysis analyzeImbalances(const Board& b) {
    ImbalanceAnalysis ia = {};
    
    // Material using public methods
    int piece_values[5] = {100, 320, 330, 500, 900};  // P, N, B, R, Q
    for (int pt = 0; pt < 5; pt++) {
        int white_count = __builtin_popcountll(b.getPieces(WHITE, pt + 1));
        int black_count = __builtin_popcountll(b.getPieces(BLACK, pt + 1));
        ia.material += (white_count - black_count) * piece_values[pt];
    }
    
    // Pawn structure assessment
    uint64_t white_pawns = b.getPieces(WHITE, PAWNS);
    uint64_t black_pawns = b.getPieces(BLACK, PAWNS);
    
    // Passed pawns
    uint64_t temp = white_pawns;
    while (temp) {
        int sq = __builtin_ctzll(temp);
        if (isPassedPawn(b, WHITE, sq)) {
            ia.white_has_passed_pawn = true;
            ia.pawn_structure += 30;
        }
        if (isIsolatedPawn(b, WHITE, sq)) {
            ia.white_has_isolated = true;
            ia.pawn_structure -= 20;
        }
        temp &= temp - 1;
    }
    
    temp = black_pawns;
    while (temp) {
        int sq = __builtin_ctzll(temp);
        if (isPassedPawn(b, BLACK, sq)) {
            ia.black_has_passed_pawn = true;
            ia.pawn_structure -= 30;
        }
        if (isIsolatedPawn(b, BLACK, sq)) {
            ia.black_has_isolated = true;
            ia.pawn_structure += 20;
        }
        temp &= temp - 1;
    }
    
    // Space using public methods
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
    
    // Development
    uint64_t white_non_pawns = b.getPieces(WHITE, KNIGHTS) | b.getPieces(WHITE, BISHOPS) |
                              b.getPieces(WHITE, ROOKS) | b.getPieces(WHITE, QUEENS);
    uint64_t black_non_pawns = b.getPieces(BLACK, KNIGHTS) | b.getPieces(BLACK, BISHOPS) |
                              b.getPieces(BLACK, ROOKS) | b.getPieces(BLACK, QUEENS);
    
    uint64_t white_back_rank = (0xFFULL << 56) | 0xFFULL;
    uint64_t black_back_rank = 0xFFULL;
    
    int white_dev = __builtin_popcountll(white_non_pawns & ~white_back_rank);
    int black_dev = __builtin_popcountll(black_non_pawns & ~black_back_rank);
    ia.development = (white_dev - black_dev) * 30;
    
    // Initiative
    ia.initiative = (b.getPlayerToMove() == WHITE) ? 10 : -10;
    
    // King safety
    if (!(b.getCastlingRights() & WHITECASTLE)) {
        ia.white_king_exposed = (b.getKingSq(WHITE) >= 56);
        ia.king_safety -= 30;
    }
    if (!(b.getCastlingRights() & BLACKCASTLE)) {
        ia.black_king_exposed = (b.getKingSq(BLACK) <= 7);
        ia.king_safety += 30;
    }
    
    // Exchange sacrifice assessment (Rook vs Minor)
    // Check if one side has R+N/B vs Q or similar imbalances
    int white_rooks = __builtin_popcountll(b.getPieces(WHITE, ROOKS));
    int black_rooks = __builtin_popcountll(b.getPieces(BLACK, ROOKS));
    int white_minors = __builtin_popcountll(b.getPieces(WHITE, KNIGHTS)) + 
                       __builtin_popcountll(b.getPieces(WHITE, BISHOPS));
    int black_minors = __builtin_popcountll(b.getPieces(BLACK, KNIGHTS)) + 
                       __builtin_popcountll(b.getPieces(BLACK, BISHOPS));
    
    // Exchange sacrifice detection
    if (white_rooks < black_rooks && white_minors > black_minors) {
        ia.exchange_sacrifice = true;
        ia.exchange_discount = -180;  // R for N/B typical discount
    }
    if (black_rooks < white_rooks && black_minors > white_minors) {
        ia.exchange_sacrifice = true;
        ia.exchange_discount = 180;
    }
    
    // Pawn sacrifice assessment (pawn down for development/initiative)
    int white_pawns_count = __builtin_popcountll(white_pawns);
    int black_pawns_count = __builtin_popcountll(black_pawns);
    if (white_pawns_count < black_pawns_count && ia.development > 30) {
        ia.pawn_sacrifice = true;
    }
    if (black_pawns_count < white_pawns_count && ia.development < -30) {
        ia.pawn_sacrifice = true;
    }
    
    // Calculate positional discounts based on style
    calculatePositionalDiscounts(ia, current_style);
    
    return ia;
}

// Calculate positional discounts (material forgiveness for compensation)
void calculatePositionalDiscounts(ImbalanceAnalysis& ia, PlayingStyle style) {
    // Exchange discount varies by style
    switch (style) {
        case PlayingStyle::CLASSICAL:
            ia.exchange_discount = ia.exchange_discount;  // Normal
            break;
        case PlayingStyle::ATTACKING:
        case PlayingStyle::TACTICAL:
            ia.exchange_discount = ia.exchange_discount * 2;  // Forgive more
            ia.initiative_discount = 50;  // Initiative worth 50 centipawns
            break;
        case PlayingStyle::POSITIONAL:
        case PlayingStyle::TECHNICAL:
            ia.exchange_discount = ia.exchange_discount / 2;  // Forgive less
            break;
    }
    
    // King safety discount - exposed king = material worth less
    if (ia.black_king_exposed) {
        ia.king_safety_discount = 50;  // Black's material worth 50 less
    }
    if (ia.white_king_exposed) {
        ia.king_safety_discount = -50;
    }
}

// Exchange sacrifice value - positive means sacrifice is justified
int exchangeSacrificeValue(const Board& b, int color) {
    ImbalanceAnalysis ia = analyzeImbalances(b);
    int value = 0;
    
    // Factors favoring exchange sacrifice:
    // 1. Open lines toward enemy king
    // 2. Opponent's king exposed
    // 3. Good minor piece coordination
    // 4. Closed position (minor pieces better)
    
    uint64_t opp_king = b.getPieces(1-color, KINGS);
    int opp_king_sq = __builtin_ctzll(opp_king);
    
    // Open lines toward opponent
    if (b.getPieces(color, ROOKS) & 0xFFULL) {  // Rook on 1st rank
        value += 30;
    }
    
    // King exposed
    if ((1-color == WHITE && opp_king_sq >= 56) || (1-color == BLACK && opp_king_sq <= 7)) {
        value += 50;
    }
    
    return value;
}

// Pawn sacrifice value - positive means pawn sacrifice is justified
int pawnSacrificeValue(const Board& b, int color) {
    int value = 0;
    
    // Pawn sacrifice is good for:
    // 1. Development advantage
    // 2. Initiative
    // 3. Open lines
    
    ImbalanceAnalysis ia = analyzeImbalances(b);
    
    if (color == WHITE) {
        if (ia.development > 30) value += 20;
        if (ia.initiative > 0) value += 30;
        if (ia.space > 20) value += 20;
    } else {
        if (ia.development < -30) value += 20;
        if (ia.initiative < 0) value += 30;
        if (ia.space < -20) value += 20;
    }
    
    return value;
}

// Generate verbal explanation for a move
MoveExplanation explainMove(const Board& b, int move, const ImbalanceAnalysis& ia) {
    MoveExplanation exp = {};
    
    // Material-based reasons
    if (ia.material > 200) {
        exp.imbalance_notes.push_back("Material +" + std::to_string(ia.material/100) + ".0");
    } else if (ia.material < -200) {
        exp.imbalance_notes.push_back("Material " + std::to_string(ia.material/100) + ".0");
    }
    
    // Pawn structure reasons
    if (ia.white_has_passed_pawn) exp.imbalance_notes.push_back("Passed pawn");
    if (ia.black_has_passed_pawn) exp.imbalance_notes.push_back("Opponent passed pawn");
    if (ia.white_has_isolated) exp.imbalance_notes.push_back("Isolated pawn (weakness)");
    if (ia.black_has_isolated) exp.imbalance_notes.push_back("Opponent isolated pawn");
    
    // Exchange sacrifice
    if (ia.exchange_sacrifice) {
        std::stringstream ss;
        ss << "Exchange sacrifice (" << (ia.exchange_discount < 0 ? "+" : "-") 
           << abs(ia.exchange_discount) << ")";
        exp.sacrifice_notes.push_back(ss.str());
        exp.imbalance_notes.push_back("R for minor piece");
    }
    
    // Initiative
    if (ia.initiative > 15) {
        exp.imbalance_notes.push_back("Strong initiative");
        exp.move_reasons.push_back("Maintaining initiative");
    }
    
    // King safety
    if (ia.white_king_exposed) {
        exp.imbalance_notes.push_back("King safety concern");
        exp.move_reasons.push_back("Kingside needs attention");
    }
    if (ia.black_king_exposed) {
        exp.imbalance_notes.push_back("Opposing king exposed");
        exp.move_reasons.push_back("Attack the exposed king");
    }
    
    // Space
    if (ia.space > 30) exp.move_reasons.push_back("More space for pieces");
    if (ia.space < -30) exp.move_reasons.push_back("Less space - expand");
    
    // Development
    if (ia.development > 60) exp.move_reasons.push_back("Better development");
    if (ia.development < -60) exp.move_reasons.push_back("Underdeveloped");
    
    // Pawn sacrifice
    if (ia.pawn_sacrifice) {
        exp.sacrifice_notes.push_back("Pawn sacrifice for development");
    }
    
    // Generate PV explanation string
    exp.pv_explanation = "";
    
    // Add sacrifice notes first
    for (size_t i = 0; i < exp.sacrifice_notes.size(); i++) {
        if (!exp.pv_explanation.empty()) exp.pv_explanation += " | ";
        exp.pv_explanation += exp.sacrifice_notes[i];
    }
    
    // Then move reasons
    for (size_t i = 0; i < exp.move_reasons.size(); i++) {
        if (!exp.pv_explanation.empty()) exp.pv_explanation += " | ";
        exp.pv_explanation += exp.move_reasons[i];
    }
    
    if (exp.pv_explanation.empty()) {
        if (!exp.imbalance_notes.empty()) {
            exp.pv_explanation = exp.imbalance_notes[0];
        } else {
            exp.pv_explanation = "Developing move";
        }
    }
    
    return exp;
}

// Endgame principle: "Do not hurry" (Shereshevsky)
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

// Initiative bonus (Sokolov-style)
int initiativeBonus(const Board& b, int color) {
    int bonus = 0;
    if (b.getPlayerToMove() == color) bonus += 10;
    
    uint64_t color_pieces = b.getPieces(color, KNIGHTS) | b.getPieces(color, BISHOPS) |
                           b.getPieces(color, ROOKS) | b.getPieces(color, QUEENS);
    int active_pieces = __builtin_popcountll(color_pieces);
    bonus += active_pieces * 5;
    return bonus;
}

// Prophylaxis assessment (Russian School)
int prophylaxisBonus(const Board& b, int color) {
    uint64_t opp_pawns = b.getPieces(1-color, PAWNS);
    int file_a = __builtin_popcountll(opp_pawns & 0x0101010101010101ULL);
    int file_h = __builtin_popcountll(opp_pawns & 0x8080808080808080ULL);
    return (file_a == 0 || file_h == 0) ? 10 : 0;
}

} // namespace HumanEval
