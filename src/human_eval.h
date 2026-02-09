/*
    Human Chess Engine - Comprehensive Chess Knowledge Implementation
    Based on Laser by Jeffrey An and Michael An
    
    Implements GM-level strategic understanding:
    - Silman-style imbalances
    - Shereshevsky endgame principles
    - Russian School prophylaxis
    - Sokolov initiative evaluation
    - Typical positional plans
*/

#ifndef HUMAN_EVAL_H
#define HUMAN_EVAL_H

#include <vector>
#include <string>
#include "board.h"

namespace HumanEval {

// ============================================================================
// ENUMS AND STRUCTURES
// ============================================================================

// Playing styles affect evaluation weighting
enum class PlayingStyle {
    CLASSICAL,    // Balanced
    ATTACKING,    // Exaggerate initiative, forgive exchanges
    TACTICAL,     // Exaggerate tactics, threaten-based
    POSITIONAL,   // Emphasize structure, patient play
    TECHNICAL     // Endgame focus, "do not hurry"
};

// Pawn structure analysis
struct PawnStructure {
    int isolated_count = 0;       // Isolated pawns
    int doubled_count = 0;       // Doubled pawns
    int backward_count = 0;      // Backward pawns
    int passed_count = 0;        // Passed pawns
    int candidate_count = 0;     // Candidate passed pawns
    int island_count = 0;         // Number of pawn islands
    int avg_island_size = 0;      // Average island size
    int connected_count = 0;      // Connected pawns
    int phalanx_count = 0;        // Adjacent pawn pairs
    bool has_chain = false;      // Has pawn chain
    int chain_base = -1;         // Base of pawn chain
    int chain_direction = 0;     // Direction of chain
};

// Piece activity analysis
struct PieceActivity {
    int knight_activity = 0;
    int bishop_activity = 0;
    int rook_activity = 0;
    int queen_activity = 0;
    int total_activity = 0;
    
    // Specific assessments
    bool has_outpost_knight = false;
    bool has_bishop_long_diagonal = false;
    bool has_rook_7th_rank = false;
    bool has_rook_open_file = false;
    bool has_queen_central = false;
};

// Main imbalance analysis (Silman-style)
struct ImbalanceAnalysis {
    // Core imbalances
    int material = 0;           // Material difference (centipawns)
    int pawn_structure = 0;     // Pawn structure assessment
    int space = 0;              // Space advantage
    int development = 0;        // Development difference
    int initiative = 0;         // Who has the initiative
    int king_safety = 0;        // King safety comparison
    int activity = 0;          // Piece activity difference
    
    // Detailed pawn structure
    PawnStructure white_pawns;
    PawnStructure black_pawns;
    
    // Piece activity
    PieceActivity white_activity;
    PieceActivity black_activity;
    
    // Specific assessments
    bool white_has_passed_pawn = false;
    bool black_has_passed_pawn = false;
    bool white_has_isolated = false;
    bool black_has_isolated = false;
    bool white_has_doubled = false;
    bool black_has_doubled = false;
    bool white_king_exposed = false;
    bool black_king_exposed = false;
    
    // Sacrifice assessments
    bool exchange_sacrifice = false;   // R for minor piece
    bool pawn_sacrifice = false;        // Pawn for development/initiative
    bool piece_sacrifice = false;        // Piece for attack
    
    // Positional discounts
    int exchange_discount = 0;          // How much R<N/B is "worth"
    int initiative_discount = 0;       // Initiative worth
    int king_safety_discount = 0;       // Exposed king discount
    
    // Typical plans detected
    bool minority_attack = false;       // Minority queenside attack
    bool open_file = false;             // Has open file
    bool rook_on_7th = false;           // Rook on 7th rank
    bool wrong_rook_pawn = false;       // Wrong rook's pawn in endgame
    bool opposite_castling = false;     // Opposite castling
    bool pawn_storm = false;            // Pawn storm possible
    int pawn_storm_strength = 0;        // How strong the pawn storm is
    
    // Endgame-specific
    bool is_endgame = false;           // Is this an endgame?
    int king_activity_white = 0;        // White king activity
    int king_activity_black = 0;        // Black king activity
    int opposition_status = 0;          // 1=white has, -1=black has, 0=none
};

// Verbal explanation for a move
struct MoveExplanation {
    std::vector<std::string> move_reasons;      // Why this move is good
    std::vector<std::string> imbalance_notes;   // Overall position assessment
    std::vector<std::string> sacrifice_notes;   // Notes about sacrifices
    std::vector<std::string> plan_notes;        // Plan descriptions
    std::string pv_explanation;                 // "I play this because..."
};

// ============================================================================
// STYLE MANAGEMENT
// ============================================================================

void setStyle(PlayingStyle style);
PlayingStyle getStyle();
const double* getStyleMultipliers();

// ============================================================================
// PAWN STRUCTURE ANALYSIS
// ============================================================================

// Check if pawn is isolated (no friendly pawns on adjacent files)
bool isIsolatedPawn(const Board& b, int color, int sq);

// Check if pawn is doubled (another pawn behind it on same file)
bool isDoubledPawn(const Board& b, int color, int sq);

// Check if pawn is backward (can't be guarded, exposed to attack)
bool isBackwardPawn(const Board& b, int color, int sq);

// Check if pawn is passed
bool isPassedPawn(const Board& b, int color, int sq);

// Check if pawn is a candidate (potentially passed)
bool isCandidatePawn(const Board& b, int color, int sq);

// Analyze pawn structure for a side
void analyzePawnStructure(const Board& b, int color, PawnStructure& ps);

// ============================================================================
// PIECE ACTIVITY ANALYSIS
// ============================================================================

// Evaluate knight activity
int evaluateKnight(const Board& b, int color, int sq);

// Evaluate bishop activity
int evaluateBishop(const Board& b, int color, int sq);

// Evaluate rook activity
int evaluateRook(const Board& b, int color, int sq);

// Evaluate queen activity
int evaluateQueen(const Board& b, int color, int sq);

// Analyze piece activity for a side
void analyzePieceActivity(const Board& b, int color, PieceActivity& pa);

// ============================================================================
// KING SAFETY ANALYSIS
// ============================================================================

// Evaluate king safety
int evaluateKingSafety(const Board& b, int color);

// ============================================================================
// INITIATIVE AND TEMPO ("Right to Move")
// ============================================================================
//
// From Euwe/Kramer and Aagaard:
// - Initiative is NOT just having the move - it's the ABILITY to make
//   consecutive forcing moves
// - "The initiative is a tempo advantage - the ability to force the
//   opponent to react to your threats"
// - Who has the "last word" on pawn breaks?
// - Count forcing moves (checks, captures, threats)

// Count available forcing moves (checks, captures, direct threats)
int countForcingMoves(const Board& b, int color);

// Assess who has the "last word" on pawn breaks
int assessPawnBreakTiming(const Board& b, int color);

// Evaluate initiative (who has the initiative - "right to move")
int evaluateInitiative(const Board& b, int color);

// ============================================================================
// TYPICAL PLANS DETECTION
// ============================================================================

// Detect minority attack (e.g., queenside pawns vs kingside pawns)
bool detectMinorityAttack(const Board& b, int color);

// Detect if side has open file for rooks
bool detectOpenFile(const Board& b, int color);

// Detect rook on open file
bool detectRookOnOpenFile(const Board& b, int color);

// Detect rook on 7th rank
bool detectRookOn7th(const Board& b, int color);

// ============================================================================
// ENDGAME PRINCIPLES (SHERESHEVSKY)
// ============================================================================
//
// From Shereshevsky's endgame teachings:
// - King centrality: kings should head toward the center in endgames
// - The Opposition: who controls the key squares
// - Key squares: critical squares for king/pawn battles
// - "Do not hurry": patient play wins endgames
// - Wrong rook's back: rook should be behind its pawn, opposite color

// Opposition types
enum class OppositionType {
    NONE,           // No opposition
    DIRECT,         // Kings on same file/rank with odd distance
    DISTANT,        // Kings separated by 2+ squares  
    DIAGONAL,       // Kings on same diagonal
    HOLE            // Can give opposition
};

// Determine opposition type
OppositionType getOppositionType(const Board& b);

// Evaluate king activity in endgame (centrality)
int evaluateEndgameKing(const Board& b, int color);

// Check opposition status
int evaluateOpposition(const Board& b, int color);

// Evaluate rook placement (right rook's back)
int evaluateRookPlacement(const Board& b, int color);

// Evaluate "do not hurry" patience principle
int evaluatePatience(const Board& b, int color);

// Comprehensive endgame evaluation
int evaluateEndgame(const Board& b, int color);

// ============================================================================
// MATERIAL IMBALANCE
// ============================================================================

// Detect exchange sacrifice (R for N/B)
bool detectExchangeSacrifice(const Board& b, int color, int& discount);

// Detect queen for R+N/B
bool detectQueenTrade(const Board& b, int color, int& discount);

// ============================================================================
// ATTACKING PATTERNS
// ============================================================================

// Detect opposite castling (both sides castled on opposite flanks)
bool detectOppositeCastling(const Board& b);

// Evaluate pawn storm potential when opposite castling
int evaluatePawnStorm(const Board& b, int color);

// Detect if king is vulnerable to pawn storm
bool isKingVulnerableToStorm(const Board& b, int color);

// ============================================================================
// MAIN EVALUATION FUNCTIONS
// ============================================================================

// Analyze position imbalances (Silman-style)
ImbalanceAnalysis analyzeImbalances(const Board& b);

// Generate verbal explanation for why a move is good
MoveExplanation explainMove(const Board& b, int move, const ImbalanceAnalysis& ia);

// Endgame principle: "Do not hurry" (Shereshevsky)
int endgamePatienceBonus(const Board& b, int color);

// Initiative bonus (Sokolov-style)
int initiativeBonus(const Board& b, int color);

// Prophylaxis assessment (Russian School)
int prophylaxisBonus(const Board& b, int color);

// Calculate positional discounts based on style
void calculatePositionalDiscounts(ImbalanceAnalysis& ia, PlayingStyle style);

// ============================================================================
// STYLE-SPECIFIC EVALUATION
// ============================================================================

// Adjust evaluation based on playing style
int getStyleAdjustedEval(const ImbalanceAnalysis& ia);

// ============================================================================
// ADDITIONAL EVALUATION FUNCTIONS
// ============================================================================

// Evaluate control of key squares
int evaluateKeySquares(const Board& b, int color);

// Evaluate piece coordination
int evaluatePieceCoordination(const Board& b, int color);

// Prophylactic thinking assessment
int evaluateProphylaxis(const Board& b, int color);

// ============================================================================
// PAWN BREAK TIMING (EUWE/KRAMER)
// ============================================================================

// Assess pawn break opportunities and timing
int evaluatePawnBreaks(const Board& b, int color);

// ============================================================================
// CONVERSION MODE (AAGAARD)
// ============================================================================

// Evaluate when to switch from dynamic to static play
int evaluateConversionMode(const Board& b, int color);

} // namespace HumanEval

#endif
