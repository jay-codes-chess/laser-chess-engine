/*
    Human Chess Engine - Teaching Evaluation Module
    Based on Laser by Jeffrey An and Michael An
    
    Adds Silman-style imbalances and teaching explanations.
*/

#ifndef HUMAN_EVAL_H
#define HUMAN_EVAL_H

#include <vector>
#include <string>
#include "board.h"

namespace HumanEval {

// Silman-style imbalance analysis
struct ImbalanceAnalysis {
    int material;           // Material difference (centipawns)
    int pawn_structure;     // Pawn structure (+/- 20-50)
    int space;              // Space advantage
    int development;        // Development difference
    int initiative;         // Who has the initiative
    int king_safety;        // King safety comparison
    
    // Specific assessments
    bool white_has_passed_pawn;
    bool black_has_passed_pawn;
    bool white_has_isolated;
    bool black_has_isolated;
    bool white_has_doubled;
    bool black_has_doubled;
    bool white_king_exposed;
    bool black_king_exposed;
    bool static_advantage_white;  // Good for white's pieces
    bool static_advantage_black;  // Good for black's pieces
    
    // Sacrifice assessments
    bool exchange_sacrifice;      // Rook for minor piece
    bool pawn_sacrifice;          // Pawn down for development/initiative
    bool piece_sacrifice;         // Piece for attack
    
    // Positional discounts (material forgiveness for compensation)
    int exchange_discount;        // How much R<N/B is "worth" (typically 0-180)
    int initiative_discount;      // Material worth giving up for initiative
    int king_safety_discount;      // Material worth less if king exposed
};

// Verbal explanation for a move
struct MoveExplanation {
    std::vector<std::string> move_reasons;      // Why this move is good
    std::vector<std::string> imbalance_notes;   // Overall position assessment
    std::vector<std::string> sacrifice_notes;   // Notes about sacrifices
    std::string pv_explanation;                 // "I play this because..."
};

// Analyze position imbalances (Silman-style)
ImbalanceAnalysis analyzeImbalances(const Board& b);

// Generate verbal explanation for why a move is good
MoveExplanation explainMove(const Board& b, int move, const ImbalanceAnalysis& ia);

// Set playing style (affects evaluation weighting)
enum class PlayingStyle {
    CLASSICAL,    // Balanced
    ATTACKING,    // Exaggerate initiative
    TACTICAL,     // Exaggerate tactics/threats
    POSITIONAL,   // Exaggerate pawn structure, forgive exchanges
    TECHNICAL     // Patient, "do not hurry"
};

void setStyle(PlayingStyle style);
PlayingStyle getStyle();

// Style multipliers (applied to eval components)
const double* getStyleMultipliers();

// Endgame principle: "Do not hurry" (Shereshevsky)
int endgamePatienceBonus(const Board& b, int color);

// Initiative bonus (Sokolov-style strategic sacrifices)
int initiativeBonus(const Board& b, int color);

// Prophylaxis assessment (Russian School)
int prophylaxisBonus(const Board& b, int color);

// Exchange sacrifice assessment
// Returns positive value if sacrificing the exchange is justified
int exchangeSacrificeValue(const Board& b, int color);

// Pawn sacrifice assessment
// Returns positive value if sacrificing a pawn is justified
int pawnSacrificeValue(const Board& b, int color);

// Calculate positional discounts based on style
// Returns discounts to material value based on compensation
void calculatePositionalDiscounts(ImbalanceAnalysis& ia, PlayingStyle style);

} // namespace HumanEval

#endif
