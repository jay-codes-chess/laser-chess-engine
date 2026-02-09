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
    int material;           // Material difference
    int pawn_structure;     // Pawn structure assessment  
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
    bool static_advantage_white;  // Good for whites pieces
    bool static_advantage_black;  // Good for black pieces
};

// Verbal explanation for a move
struct MoveExplanation {
    std::vector<std::string> move_reasons;      // Why this move is good
    std::vector<std::string> imbalance_notes;   // Overall position assessment
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
    POSITIONAL,   // Exaggerate pawn structure
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

} // namespace HumanEval

#endif
