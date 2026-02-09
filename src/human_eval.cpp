/*
    Human Chess Engine - Comprehensive Chess Knowledge Implementation
*/

#include "human_eval.h"
#include "board.h"
#include "eval.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <queue>

namespace HumanEval {

static PlayingStyle current_style = PlayingStyle::CLASSICAL;
static double classical_mults[2] = {1.0, 1.0};
static double attacking_mults[2] = {1.2, 0.8};
static double tactical_mults[2] = {1.3, 0.6};
static double positional_mults[2] = {0.7, 1.3};
static double technical_mults[2] = {0.6, 1.4};

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

// Knight outpost table
static const int KNIGHT_OUTPOST[64] = {
    -5,-5,-5,-5,-5,-5,-5,-5,-5,0,0,0,0,0,0,-5,-5,0,5,5,5,5,0,-5,-5,0,5,10,10,5,0,-5,
    -5,0,5,10,10,5,0,-5,-5,0,3,5,5,3,0,-5,-5,0,0,0,0,0,0,-5,-5,-5,-5,-5,-5,-5,-5,-5
};

static const int BISHOP_LONG_DIAGONAL[64] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,5,5,0,0,0,0,5,10,10,5,0,0,0,0,5,10,15,5,0,0,
    0,0,5,10,10,5,0,0,0,0,5,5,5,5,0,0,0,0,0,0,0,0,0,0
};

static const int ROOK_7TH_RANK[64] = {
    0,0,0,0,0,0,0,0,10,10,10,10,10,10,10,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

// ============================================================================
// PAWN STRUCTURE
// ============================================================================

bool isIsolatedPawn(const Board& bd, int color, int sq) {
    int file = sq % 8;
    uint64_t own = bd.getPieces(color, PAWNS);
    if (file > 0) {
        uint64_t left = 0; for (int r = 0; r < 8; r++) left |= 1ULL << (file - 1 + r * 8);
        if (own & left) return false;
    }
    if (file < 7) {
        uint64_t right = 0; for (int r = 0; r < 8; r++) right |= 1ULL << (file + 1 + r * 8);
        if (own & right) return false;
    }
    return true;
}

bool isDoubledPawn(const Board& bd, int color, int sq) {
    int file = sq % 8, rank = sq / 8;
    uint64_t own = bd.getPieces(color, PAWNS);
    for (int r = (color == WHITE ? 0 : rank + 1); r < rank; r++)
        if (own & (1ULL << (file + r * 8))) return true;
    return false;
}

bool isBackwardPawn(const Board& bd, int color, int sq) {
    int file = sq % 8, rank = sq / 8;
    uint64_t own = bd.getPieces(color, PAWNS);
    uint64_t opp = bd.getPieces(1-color, PAWNS);
    int fwd = (color == WHITE) ? 8 : -8;
    int psq = sq + fwd;
    if (psq < 0 || psq >= 64) return false;
    bool prot = false;
    if (file > 0) { int a = (color == WHITE) ? psq - 1 : psq + 1; if (a >= 0 && (own & (1ULL << a))) prot = true; }
    if (file < 7) { int a = (color == WHITE) ? psq + 1 : psq - 1; if (a < 64 && (own & (1ULL << a))) prot = true; }
    uint64_t att = (color == WHITE) ? 
        ((opp >> 9) & 0xFEFEFEFEFEFEFEFEULL) | ((opp >> 7) & 0x7F7F7F7F7F7F7F7FULL) :
        ((opp << 9) & 0x7F7F7F7F7F7F7F7FULL) | ((opp << 7) & 0xFEFEFEFEFEFEFEFEULL);
    return !prot && (att & (1ULL << sq));
}

bool isPassedPawn(const Board& bd, int color, int sq) {
    int file = sq % 8, rank = sq / 8;
    uint64_t own = bd.getPieces(color, PAWNS), opp = bd.getPieces(1-color, PAWNS);
    for (int f = file - 1; f >= 0; f--) if (own & (1ULL << (f * 8 + rank))) return false;
    for (int f = file + 1; f < 8; f++) if (own & (1ULL << (f * 8 + rank))) return false;
    int cr = (color == WHITE) ? rank + 1 : rank - 1;
    if (cr >= 0 && cr < 8) {
        uint64_t m = 0; if (file > 0) m |= 1ULL << (file - 1 + cr * 8);
        if (file < 7) m |= 1ULL << (file + 1 + cr * 8);
        if (opp & m) return false;
    }
    return true;
}

bool isCandidatePawn(const Board& bd, int color, int sq) {
    int file = sq % 8, rank = sq / 8;
    uint64_t own = bd.getPieces(color, PAWNS), opp = bd.getPieces(1-color, PAWNS);
    int fwd = (color == WHITE) ? 8 : -8;
    int bs = sq + fwd;
    if (bs >= 0 && bs < 64 && (own & (1ULL << bs))) return false;
    int fr = (color == WHITE) ? rank + 1 : rank - 1;
    if (fr >= 0 && fr < 8) {
        uint64_t fm = 0; if (file > 0) fm |= 1ULL << (file - 1 + fr * 8);
        if (file < 7) fm |= 1ULL << (file + 1 + fr * 8);
        if (opp & fm) {
            int ar = (color == WHITE) ? rank + 2 : rank - 2;
            if (ar >= 0 && ar < 8) {
                uint64_t am = 0; if (file > 0) am |= 1ULL << (file - 1 + ar * 8);
                if (file < 7) am |= 1ULL << (file + 1 + ar * 8);
                if (!(opp & am)) return true;
            }
        }
    }
    return isPassedPawn(bd, color, sq);
}

void countPawnIslands(const Board& bd, int color, int& islands, int& avg) {
    uint64_t p = bd.getPieces(color, PAWNS);
    bool v[64] = {false};
    std::vector<int> sz;
    while (p) {
        int sq = __builtin_ctzll(p); p &= p - 1;
        if (v[sq]) continue;
        std::vector<int> c; std::queue<int> q; q.push(sq); v[sq] = true;
        while (!q.empty()) {
            int cur = q.front(); q.pop(); c.push_back(cur);
            int f = cur % 8, r = cur / 8;
            const int dx[4] = {-1,1,0,0}, dy[4] = {0,0,-1,1};
            for (int i = 0; i < 4; i++) {
                int nf = f + dx[i], nr = r + dy[i];
                if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
                    int ns = nf + nr * 8;
                    if ((bd.getPieces(color, PAWNS) & (1ULL << ns)) && !v[ns]) { v[ns] = true; q.push(ns); }
                }
            }
        }
        if (!c.empty()) sz.push_back(c.size());
    }
    islands = sz.size();
    int t = 0; for (int s : sz) t += s;
    avg = islands > 0 ? t / islands : 0;
}

void analyzePawnStructure(const Board& bd, int color, PawnStructure& ps) {
    uint64_t p = bd.getPieces(color, PAWNS);
    ps.isolated_count = ps.doubled_count = ps.backward_count = 0;
    ps.passed_count = ps.candidate_count = 0;
    ps.island_count = ps.connected_count = ps.phalanx_count = 0;
    ps.has_chain = false;
    
    uint64_t t = p;
    while (t) {
        int sq = __builtin_ctzll(t); t &= t - 1;
        if (isIsolatedPawn(bd, color, sq)) ps.isolated_count++;
        if (isDoubledPawn(bd, color, sq)) ps.doubled_count++;
        if (isBackwardPawn(bd, color, sq)) ps.backward_count++;
        if (isPassedPawn(bd, color, sq)) ps.passed_count++;
        if (isCandidatePawn(bd, color, sq)) ps.candidate_count++;
    }
    ps.connected_count = __builtin_popcountll(p) - ps.isolated_count;
    countPawnIslands(bd, color, ps.island_count, ps.avg_island_size);
    
    t = p;
    while (t) {
        int sq = __builtin_ctzll(t); int f = sq % 8; t &= t - 1;
        if (f < 7 && (p & (1ULL << (sq + 1)))) ps.phalanx_count++;
    }
    
    int minr = 8, maxr = -1;
    t = p;
    while (t) {
        int sq = __builtin_ctzll(t); int r = sq / 8; t &= t - 1;
        minr = std::min(minr, r); maxr = std::max(maxr, r);
    }
    if (maxr - minr >= 2) { ps.has_chain = true; ps.chain_base = (color == WHITE) ? maxr : minr; }
}

// ============================================================================
// PIECE ACTIVITY
// ============================================================================

int evaluateKnight(const Board& bd, int color, int sq) {
    int s = KNIGHT_OUTPOST[sq];
    int f = sq % 8, r = sq / 8;
    return s + (7 - (abs(f - 3) + abs(r - 3))) * 3;
}

int evaluateBishop(const Board& bd, int color, int sq) {
    int s = BISHOP_LONG_DIAGONAL[sq];
    int f = sq % 8, r = sq / 8;
    s += (7 - (abs(f - 3) + abs(r - 3))) * 3;
    if (__builtin_popcountll(bd.getPieces(color, BISHOPS)) >= 2) s += 30;
    return s;
}

int evaluateRook(const Board& bd, int color, int sq) {
    int s = 0, f = sq % 8, r = sq / 8;
    if ((color == WHITE && r == 6) || (color == BLACK && r == 1)) s += ROOK_7TH_RANK[sq];
    uint64_t pawns = bd.getPieces(color, PAWNS);
    uint64_t fm = 0; for (int i = 0; i < 8; i++) fm |= 1ULL << (f + i * 8);
    if (!(pawns & fm)) s += 20;
    else if (!(pawns & (1ULL << (color == WHITE ? 56 + f : f)))) s += 10;
    return s;
}

int evaluateQueen(const Board& bd, int color, int sq) {
    int f = sq % 8, r = sq / 8;
    int s = (7 - (abs(f - 3) + abs(r - 3))) * 4;
    if ((color == WHITE && r > 3) || (color == BLACK && r < 4))
        if (bd.getPieces(color, KNIGHTS) || bd.getPieces(color, BISHOPS)) s -= 15;
    return s;
}

void analyzePieceActivity(const Board& bd, int color, PieceActivity& pa) {
    pa.total_activity = 0;
    pa.has_outpost_knight = pa.has_bishop_long_diagonal = false;
    pa.has_rook_7th_rank = pa.has_rook_open_file = false;
    
    uint64_t n = bd.getPieces(color, KNIGHTS);
    while (n) { int sq = __builtin_ctzll(n); n &= n - 1; 
        int v = evaluateKnight(bd, color, sq); pa.total_activity += v;
        if (v > 10) pa.has_outpost_knight = true; }
    
    uint64_t bi = bd.getPieces(color, BISHOPS);
    while (bi) { int sq = __builtin_ctzll(bi); bi &= bi - 1; 
        int v = evaluateBishop(bd, color, sq); pa.total_activity += v;
        if (v > 10) pa.has_bishop_long_diagonal = true; }
    
    uint64_t r = bd.getPieces(color, ROOKS);
    while (r) { int sq = __builtin_ctzll(r); r &= r - 1; 
        int v = evaluateRook(bd, color, sq); pa.total_activity += v;
        if (v > 15) pa.has_rook_7th_rank = true;
        if (v >= 20) pa.has_rook_open_file = true; }
    
    uint64_t q = bd.getPieces(color, QUEENS);
    while (q) { int sq = __builtin_ctzll(q); q &= q - 1; 
        pa.total_activity += evaluateQueen(bd, color, sq); }
}

// ============================================================================
// KING SAFETY
// ============================================================================

int evaluateKingSafety(const Board& bd, int color) {
    int s = 0, ks = bd.getKingSq(color), f = ks % 8;
    bool can_castle = (bd.getCastlingRights() & (color == WHITE ? WHITECASTLE : BLACKCASTLE));
    if (!can_castle) {
        if ((color == WHITE && ks >= 56) || (color == BLACK && ks <= 7)) s -= 25;
        uint64_t pawns = bd.getPieces(color, PAWNS);
        if (color == WHITE) { if (pawns & (1ULL << 5)) s += 5; if (pawns & (1ULL << 6)) s += 5; }
        else { if (pawns & (1ULL << 61)) s += 5; if (pawns & (1ULL << 62)) s += 5; }
        if (f <= 1 || f >= 6) s -= 10;
    } else s += 20;
    return s;
}

// ============================================================================
// INITIATIVE - "The Right to Move"
// ============================================================================
//
// Initiative is NOT just having the move - it's the ABILITY to make consecutive
// forcing moves. This is the "right to move" concept from Euwe/Kramer.
//
// Key ideas Forcing moves = checks, captures,:
// 1. threats
// 2. Who has MORE forcing moves available?
// 3. Pawn breaks - who has the "last word"?
// 4. Initiative transfer - when does pressure switch sides?
//
// From Aagaard: "The initiative is a tempo advantage - the ability to force
// the opponent to react to your threats."

// Count available forcing moves (checks, captures, direct threats)
int countForcingMoves(const Board& bd, int color) {
    int count = 0;
    
    // Get all piece types
    uint64_t knights = bd.getPieces(color, KNIGHTS);
    uint64_t bishops = bd.getPieces(color, BISHOPS);
    uint64_t rooks = bd.getPieces(color, ROOKS);
    uint64_t queens = bd.getPieces(color, QUEENS);
    uint64_t pawns = bd.getPieces(color, PAWNS);
    
    // Simplified: count pieces that can give check or capture
    // Real implementation would generate moves, but this is approximation
    
    // Knights giving check
    count += __builtin_popcountll(knights);
    
    // Bishops/Queens on long diagonal = checking threat
    uint64_t bq = bishops | queens;
    uint64_t temp = bq;
    while (temp) {
        int sq = __builtin_ctzll(temp);
        temp &= temp - 1;
        int f = sq % 8, r = sq / 8;
        // Center control = initiative
        if (f >= 2 && f <= 5 && r >= 2 && r <= 5) count += 2;
        else count += 1;
    }
    
    // Rooks on open files = threat
    uint64_t rq = rooks | queens;
    temp = rq;
    while (temp) {
        int sq = __builtin_ctzll(temp);
        temp &= temp - 1;
        int f = sq % 8;
        uint64_t fm = 0; for (int rk = 0; rk < 8; rk++) fm |= 1ULL << (f + rk * 8);
        if (!(pawns & fm)) count += 2; // Open file rook
        else count += 1;
    }
    
    // Advanced pawns = pawn break threats
    uint64_t adv_pawns = 0;
    if (color == WHITE) {
        adv_pawns = (pawns & 0xFF000000000000ULL) >> 56; // Rank 7+
    } else {
        adv_pawns = (pawns & 0xFFULL) << 56; // Rank 2+
    }
    count += __builtin_popcountll(adv_pawns) * 2;
    
    return count;
}

// Assess who has the "last word" on pawn breaks
// The side that can make the LAST forcing pawn move has the initiative
int assessPawnBreakTiming(const Board& bd, int color) {
    int score = 0;
    
    // Common pawn breaks in center
    // e4/d4 breaks by White, e5/d5 breaks by Black
    
    uint64_t white_pawns = bd.getPieces(WHITE, PAWNS);
    uint64_t black_pawns = bd.getPieces(BLACK, PAWNS);
    
    // White's e4/d4 break potential
    bool white_e4_ready = (white_pawns & (1ULL << 12)); // e2 pawn
    bool white_d4_ready = (white_pawns & (1ULL << 11)); // d2 pawn
    bool black_e5_ready = (black_pawns & (1ULL << 52)); // e7 pawn
    bool black_d5_ready = (black_pawns & (1ULL << 51)); // d7 pawn
    
    // If White has e4 ready and Black can't respond with e5, White has the break
    if (white_e4_ready && !black_e5_ready) {
        score += 15; // White can break
    }
    if (white_d4_ready && !black_d5_ready) {
        score += 15; // White can break
    }
    
    // Counter-breaks
    if (black_e5_ready && !white_e4_ready) {
        score -= 15; // Black can break
    }
    if (black_d5_ready && !white_d4_ready) {
        score -= 15; // Black can break
    }
    
    return score;
}

// Enhanced initiative evaluation
// Initiative = The right to make consecutive forcing moves
int evaluateInitiative(const Board& bd, int color) {
    int s = 0;
    
    // 1. Basic tempo: having the move
    // But "having the move" only matters if you have forcing moves!
    bool has_move = (bd.getPlayerToMove() == color);
    int forcing_moves = countForcingMoves(bd, color);
    int opp_forcing_moves = countForcingMoves(bd, 1-color);
    
    if (has_move) {
        // If you have the move AND more forcing moves = initiative
        if (forcing_moves > opp_forcing_moves) {
            s += 20; // Clear initiative
        } else if (forcing_moves == opp_forcing_moves) {
            s += 10; // Slight edge for having the move
        } else {
            s += 5; // Having the move helps even with equal forcing
        }
    } else {
        // Opponent has the move
        if (opp_forcing_moves > forcing_moves + 2) {
            s -= 10; // Opponent has clear initiative
        } else if (opp_forcing_moves > forcing_moves) {
            s -= 5; // Opponent has slight initiative
        } else {
            s += 0; // Equal
        }
    }
    
    // 2. Active pieces contribute to initiative potential
    uint64_t br = (color == WHITE) ? (0xFFULL << 56) | 0xFFULL : 0xFFULL;
    uint64_t act = (bd.getPieces(color, KNIGHTS) | bd.getPieces(color, BISHOPS) |
                    bd.getPieces(color, ROOKS) | bd.getPieces(color, QUEENS)) & ~br;
    s += __builtin_popcountll(act) * 3;
    
    // 3. Open files = initiative potential (rook can become active)
    uint64_t pawns = bd.getPieces(color, PAWNS);
    for (int f = 0; f < 8; f++) {
        uint64_t fm = 0; for (int rk = 0; rk < 8; rk++) fm |= 1ULL << (f + rk * 8);
        if (!(pawns & fm)) s += 3;
    }
    
    // 4. Pawn break timing - who has the "last word"?
    int break_score = assessPawnBreakTiming(bd, color);
    if (color == WHITE) {
        s += break_score;
    } else {
        s -= break_score; // Invert for black
    }
    
    // 5. Space advantage = initiative
    // Control of the center restricts opponent's counterplay
    uint64_t center = (1ULL << 27) | (1ULL << 28) | (1ULL << 35) | (1ULL << 36) |
                     (1ULL << 26) | (1ULL << 37) | (1ULL << 19) | (1ULL << 44);
    uint64_t own_center = center & (bd.getPieces(color, PAWNS) | 
                                     (bd.getPieces(color, KNIGHTS) | bd.getPieces(color, BISHOPS)));
    s += __builtin_popcountll(own_center) * 2;
    
    return s;
}

// ============================================================================
// TYPICAL PLANS
// ============================================================================

bool detectMinorityAttack(const Board& bd, int color) {
    uint64_t p = bd.getPieces(color, PAWNS);
    uint64_t qs = 0, ks = 0;
    for (int r = 0; r < 8; r++) { qs |= 1ULL << (0 + r * 8); qs |= 1ULL << (1 + r * 8); qs |= 1ULL << (2 + r * 8);
        ks |= 1ULL << (5 + r * 8); ks |= 1ULL << (6 + r * 8); ks |= 1ULL << (7 + r * 8); }
    int qsc = __builtin_popcountll(p & qs), ksc = __builtin_popcountll(p & ks);
    return qsc <= 2 && qsc < ksc;
}

bool detectRookOnOpenFile(const Board& bd, int color) {
    uint64_t r = bd.getPieces(color, ROOKS), p = bd.getPieces(color, PAWNS);
    for (int f = 0; f < 8; f++) {
        uint64_t fm = 0; for (int rk = 0; rk < 8; rk++) fm |= 1ULL << (f + rk * 8);
        if (!(p & fm) && (r & fm)) return true;
    }
    return false;
}

bool detectRookOn7th(const Board& bd, int color) {
    uint64_t r = bd.getPieces(color, ROOKS);
    int rank = (color == WHITE) ? 6 : 1;
    for (int f = 0; f < 8; f++) if (r & (1ULL << (f + rank * 8))) return true;
    return false;
}

// ============================================================================
// ATTACKING PATTERNS - PAWN STORM
// ============================================================================

// Detect opposite castling (white kingside, black queenside or vice versa)
bool detectOppositeCastling(const Board& bd) {
    // White castled kingside: king on g1/h1, rooks on f1/h1 or moved
    bool white_kside = !(bd.getCastlingRights() & WHITEQSIDE);
    // Black castled queenside: king on c8/b8, rooks on a8/c8 or moved  
    bool black_qside = !(bd.getCastlingRights() & BLACKKSIDE);
    
    // White castled queenside, black castled kingside
    bool white_qside = !(bd.getCastlingRights() & WHITEKSIDE);
    bool black_kside = !(bd.getCastlingRights() & BLACKQSIDE);
    
    return (white_kside && black_qside) || (white_qside && black_kside);
}

// Count pawns that can advance toward enemy king
int countPawnStorm(const Board& bd, int color) {
    uint64_t pawns = bd.getPieces(color, PAWNS);
    int storm_count = 0;
    
    int king_sq = bd.getKingSq(1-color);
    int kf = king_sq % 8;
    int kr = king_sq / 8;
    
    // For white storming black's king, look at f,g,h pawns advancing
    // For black storming white's king, look at f,g,h pawns advancing
    
    uint64_t temp = pawns;
    while (temp) {
        int sq = __builtin_ctzll(temp);
        temp &= temp - 1;
        
        int file = sq % 8;
        int rank = sq / 8;
        
        if (color == WHITE) {
            // White pawns that can advance toward black king
            // g2, h2, f2, g3, h3, f3 can storm
            if ((file >= 5 && file <= 7) || (file >= 5 && file <= 7 && rank >= 2)) {
                int advance = 0;
                if (file == 5) { // f-pawn
                    if (rank == 1 && !(bd.getPieces(WHITE, PAWNS) & (1ULL << (5 + 2 * 8)))) advance = 3; // f2
                    else if (rank == 2) advance = 2; // f3
                } else if (file == 6) { // g-pawn
                    if (rank == 1 && !(bd.getPieces(WHITE, PAWNS) & (1ULL << (6 + 2 * 8)))) advance = 4; // g2
                    else if (rank == 2) advance = 3; // g3
                } else if (file == 7) { // h-pawn
                    if (rank == 1 && !(bd.getPieces(WHITE, PAWNS) & (1ULL << (7 + 2 * 8)))) advance = 3; // h2
                    else if (rank == 2) advance = 2; // h3
                }
                storm_count += advance;
            }
        } else {
            // Black pawns that can advance toward white king
            // f7, g7, h7, f6, g6, h6
            if ((file >= 5 && file <= 7) || (file >= 5 && file <= 7 && rank <= 5)) {
                int advance = 0;
                if (file == 5) { // f-pawn
                    if (rank == 6 && !(bd.getPieces(BLACK, PAWNS) & (1ULL << (5 + 5 * 8)))) advance = 3; // f7
                    else if (rank == 5) advance = 2; // f6
                } else if (file == 6) { // g-pawn
                    if (rank == 6 && !(bd.getPieces(BLACK, PAWNS) & (1ULL << (6 + 5 * 8)))) advance = 4; // g7
                    else if (rank == 5) advance = 3; // g6
                } else if (file == 7) { // h-pawn
                    if (rank == 6 && !(bd.getPieces(BLACK, PAWNS) & (1ULL << (7 + 5 * 8)))) advance = 3; // h7
                    else if (rank == 5) advance = 2; // h6
                }
                storm_count += advance;
            }
        }
    }
    
    return storm_count;
}

// Evaluate pawn storm potential
int evaluatePawnStorm(const Board& bd, int color) {
    if (!detectOppositeCastling(bd)) return 0;
    
    // Count storming pawns
    int storm = countPawnStorm(bd, color);
    
    // Bonus for having the pawn storm
    return storm * 5;
}

// Detect if king is vulnerable to pawn storm
bool isKingVulnerableToStorm(const Board& bd, int color) {
    if (!detectOppositeCastling(bd)) return false;
    
    int king_sq = bd.getKingSq(color);
    int file = king_sq % 8;
    
    // King on g/h files (kingside) is vulnerable to queenside pawn storm
    // King on a/b files (queenside) is vulnerable to kingside pawn storm
    
    if (color == WHITE) {
        // White king vulnerable if castled queenside (a1/b1) and black has kingside pawns
        return (file <= 1 || king_sq >= 56);
    } else {
        // Black king vulnerable if castled queenside (a8/b8) and white has kingside pawns
        return (file <= 1 || king_sq <= 7);
    }
}

// ============================================================================
// ENDGAME PRINCIPLES (SHERESHEVSKY)
// ============================================================================
//
// From Shereshevsky's "Strategic Play" and endgame books:
// 1. King centrality - kings should head toward the center in endgames
// 2. The Opposition - who controls the key squares
// 3. Key squares - critical squares for king/pawn battles
// 4. "Do not hurry" - patient play wins endgames
// 5. Wrong rook's back - rook on wrong color square for pawn

// Key squares in king and pawn endgames
// For White king: squares white king wants to control
static const int KEY_SQUARES_WHITE[64] = {
    // Files a-h, Ranks 1-8 (a1=0, h8=63)
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,5,5,0,0,0,  // Rank 4
    0,0,0,5,5,0,0,0,  // Rank 5 - key squares around center
    0,0,0,5,5,0,0,0,  // Rank 6
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

// Evaluate king centrality in endgame
// From Shereshevsky: "The king should head for the center in the endgame"
int evaluateEndgameKing(const Board& bd, int color) {
    int ks = bd.getKingSq(color), oks = bd.getKingSq(1-color);
    int kf = ks % 8, kr = ks / 8;
    int okf = oks % 8, okr = oks / 8;
    
    // Distance from center (d4/e4/d5/e5 = 26,27,34,35)
    int center_dist = abs(kf - 4) + abs(kr - 4); // Target d4 (4,3)
    int opp_center = abs(okf - 4) + abs(okr - 4);
    
    // Reward being closer to center than opponent
    int bonus = 0;
    if (center_dist < opp_center) {
        bonus = (opp_center - center_dist) * 10;
    }
    
    // Extra bonus for controlling key squares
    bonus += KEY_SQUARES_WHITE[ks];
    
    return bonus;
}

// Determine opposition type
OppositionType getOppositionType(const Board& bd) {
    int wk = bd.getKingSq(WHITE), bk = bd.getKingSq(BLACK);
    int wkf = wk % 8, wkr = wk / 8;
    int bkf = bk % 8, bkr = bk / 8;
    
    int file_diff = abs(wkf - bkf);
    int rank_diff = abs(wkr - bkr);
    
    // Not on same rank or file = diagonal opposition (if both differ by 1)
    if (file_diff == rank_diff && file_diff % 2 == 1) {
        return OppositionType::DIAGONAL;
    }
    
    // Same rank
    if (rank_diff == 0 && file_diff > 0) {
        if (file_diff % 2 == 1) return OppositionType::DIRECT;
        if (file_diff == 2) return OppositionType::DISTANT;
        if (file_diff > 2) return OppositionType::DISTANT;
    }
    
    // Same file
    if (file_diff == 0 && rank_diff > 0) {
        if (rank_diff % 2 == 1) return OppositionType::DIRECT;
        if (rank_diff == 2) return OppositionType::DISTANT;
        if (rank_diff > 2) return OppositionType::DISTANT;
    }
    
    return OppositionType::NONE;
}

// Evaluate opposition status
// Returns: positive = White has opposition, negative = Black has opposition
int evaluateOpposition(const Board& bd, int color) {
    OppositionType opp = getOppositionType(bd);
    
    // Direct opposition is most valuable
    if (opp == OppositionType::DIRECT) {
        // If it's White's move and they're in opposition = good for player to move
        // If it's Black's move and they're in opposition = bad for White
        bool white_to_move = (bd.getPlayerToMove() == WHITE);
        if (color == WHITE) {
            return white_to_move ? 30 : -30;
        } else {
            return white_to_move ? -30 : 30;
        }
    }
    
    // Distant opposition - useful for planning
    if (opp == OppositionType::DISTANT) {
        return (color == WHITE) ? 15 : -15;
    }
    
    // Diagonal opposition - temporary
    if (opp == OppositionType::DIAGONAL) {
        return (color == WHITE) ? 10 : -10;
    }
    
    return 0;
}

// Check if a pawn is a rook pawn (a or h file)
bool isRookPawn(const Board& bd, int color, int sq) {
    int file = sq % 8;
    uint64_t pawns = bd.getPieces(color, PAWNS);
    return (pawns & (1ULL << sq)) && (file == 0 || file == 7);
}

// Check if rook is correctly placed behind its pawn
// Returns: positive = good rook placement, negative = bad (wrong rook's back)
// From Euwe/Kramer: Rook should be behind its pawn, on the opposite color square
int evaluateRookPlacement(const Board& bd, int color) {
    int score = 0;
    uint64_t rooks = bd.getPieces(color, ROOKS);
    uint64_t pawns = bd.getPieces(color, PAWNS);
    
    while (rooks) {
        int rsq = __builtin_ctzll(rooks);
        rooks &= rooks - 1;
        int rf = rsq % 8, rr = rsq / 8;
        int r_color = (rr + rf) % 2; // Square color
        
        // Check pawns on same file
        for (int r = 0; r < 8; r++) {
            int psq = rf + r * 8;
            if (pawns & (1ULL << psq)) {
                int pr = r;
                int p_color = (pr + rf) % 2;
                
                // Is rook behind pawn?
                bool rook_behind = (color == WHITE && rr < pr) || (color == BLACK && rr > pr);
                
                if (rook_behind) {
                    // Same color = "wrong rook" - should be different color
                    if (r_color == p_color) {
                        score -= 25; // Wrong rook's back
                    } else {
                        score += 15; // Correct rook placement
                    }
                }
                break; // Only check front-most pawn on file
            }
        }
    }
    
    return score;
}

// Evaluate "do not hurry" principle (Shereshevsky)
// Returns bonus for patient, methodical play
int evaluatePatience(const Board& bd, int color) {
    int bonus = 0;
    
    // In endings, don't rush pawn moves
    // If you have opposition and a passed pawn, wait!
    
    // Check for winning pawn endgame
    uint64_t w_pawns = bd.getPieces(WHITE, PAWNS);
    uint64_t b_pawns = bd.getPieces(BLACK, PAWNS);
    
    int w_piece_count = __builtin_popcountll(bd.getPieces(WHITE, KNIGHTS) | bd.getPieces(WHITE, BISHOPS) | 
                                            bd.getPieces(WHITE, ROOKS) | bd.getPieces(WHITE, QUEENS));
    int b_piece_count = __builtin_popcountll(bd.getPieces(BLACK, KNIGHTS) | bd.getPieces(BLACK, BISHOPS) | 
                                            bd.getPieces(BLACK, ROOKS) | bd.getPieces(BLACK, QUEENS));
    
    bool is_pawn_endgame = (w_piece_count == 0 && b_piece_count == 0);
    
    if (is_pawn_endgame) {
        // In pure pawn endings, patience is key
        // If you have opposition, you can afford to wait
        
        // Bonus for having the opposition in pawn endgame
        OppositionType opp = getOppositionType(bd);
        if (opp == OppositionType::DIRECT) {
            bool white_to_move = (bd.getPlayerToMove() == WHITE);
            if ((color == WHITE && white_to_move) || (color == BLACK && !white_to_move)) {
                bonus += 40; // Having the move with opposition = strong
            }
        }
    }
    
    return bonus;
}

// Comprehensive endgame evaluation combining all principles
int evaluateEndgame(const Board& bd, int color) {
    int score = 0;
    
    // 1. King centrality (Shereshevsky)
    score += evaluateEndgameKing(bd, color);
    
    // 2. Opposition
    score += evaluateOpposition(bd, color);
    
    // 3. Rook placement (right rook's back)
    score += evaluateRookPlacement(bd, color);
    
    // 4. Patience (do not hurry)
    score += evaluatePatience(bd, color);
    
    return score;
}

// ============================================================================
// PROPHYLAXIS (RUSSIAN SCHOOL)
// ============================================================================
//
// From the Russian Chess School (Petrosian, Kamsky, etc.):
// Prophylaxis = Preventing the opponent's threats BEFORE they exist
// "Don't just play your own moves - stop your opponent's ideas!"
//
// Key ideas:
// 1. Identify opponent's most dangerous plan
// 2. Play a move that blocks or hinders that plan
// 3. Restrict opponent's piece mobility
// 4. Create zugzwang positions

// Prophylactic thinking: assessing how well a player restricts opponent
// Returns bonus for good prophylaxis
int evaluateProphylaxis(const Board& bd, int color) {
    int score = 0;
    int opp = 1 - color;
    
    // 1. Restrict opponent king
    int opp_king_sq = bd.getKingSq(opp);
    int opp_kf = opp_king_sq % 8, opp_kr = opp_king_sq / 8;
    
    // Bonus if opponent's king has limited squares
    uint64_t king_mobility = 0;
    int kdirs[8] = {-9,-8,-7,-1,1,7,8,9};
    for (int d : kdirs) {
        int nsq = opp_king_sq + d;
        if (nsq >= 0 && nsq < 64) {
            int nf = nsq % 8, nr = nsq / 8;
            if (abs(nf - opp_kf) <= 1 && abs(nr - opp_kr) <= 1) {
                // Check if square is controlled by our pieces
                // Simplified: just check pawn proximity
                king_mobility |= 1ULL << nsq;
            }
        }
    }
    score += (64 - __builtin_popcountll(king_mobility)) / 2;
    
    // 2. Restrict opponent pieces
    uint64_t opp_knights = bd.getPieces(opp, KNIGHTS);
    uint64_t opp_bishops = bd.getPieces(opp, BISHOPS);
    uint64_t opp_rooks = bd.getPieces(opp, ROOKS);
    uint64_t opp_queens = bd.getPieces(opp, QUEENS);
    
    // If opponent has minor pieces trapped, that's good prophylaxis
    uint64_t own_pawns = bd.getPieces(color, PAWNS);
    
    // Count opponent's minor pieces in corners/edges (restricted)
    uint64_t corners = (1ULL << 0) | (1ULL << 7) | (1ULL << 56) | (1ULL << 63);
    uint64_t edges = 0xFF000000000000FFULL | 0xFF00FF00FF00FF00ULL;
    uint64_t restricted = (opp_knights | opp_bishops) & (corners | edges);
    score += __builtin_popcountll(restricted) * 5;
    
    // 3. Control of opponent's natural squares
    // If opponent's knight has no good outposts
    uint64_t outpost_squares = 0;
    // d4, e4, d5, e5 area
    outpost_squares |= (1ULL << 27) | (1ULL << 28) | (1ULL << 35) | (1ULL << 36);
    outpost_squares |= (1ULL << 26) | (1ULL << 37) | (1ULL << 19) | (1ULL << 44);
    
    uint64_t own_control = own_pawns | bd.getPieces(color, KNIGHTS) | bd.getPieces(color, BISHOPS);
    uint64_t opp_outposts = outpost_squares & ~own_control;
    score += __builtin_popcountll(opp_outposts) * 2;
    
    return score;
}

// ============================================================================
// PAWN BREAK TIMING (EUWE/KRAMER)
// ============================================================================
//
// From Euwe & Kramer - The Middlegame:
// Pawn breaks are the "moments of truth" in chess
// - Who has the last word on pawn breaks controls the game
// - A pawn break either strengthens or weakens your position permanently
//
// Key concepts:
// - The side that can make the LAST forcing pawn move has the initiative
// - Central breaks (e4, d4, e5, d5) are most important
// - Breaking into the opponent's pawn structure
// - The "break or be broken" moment

// Assess pawn break opportunities and timing
int evaluatePawnBreaks(const Board& bd, int color) {
    int score = 0;
    int opp = 1 - color;
    
    uint64_t own_pawns = bd.getPieces(color, PAWNS);
    uint64_t opp_pawns = bd.getPieces(opp, PAWNS);
    
    // 1. Center pawn breaks (most important)
    // White: e4, d4 | Black: e5, d5
    
    bool white_e4_ready = (own_pawns & (1ULL << 12)); // e2 pawn can go to e4
    bool white_d4_ready = (own_pawns & (1ULL << 11)); // d2 pawn can go to d4
    bool black_e5_ready = (opp_pawns & (1ULL << 52)); // e7 pawn can go to e5
    bool black_d5_ready = (opp_pawns & (1ULL << 51)); // d7 pawn can go to d5
    
    // Center control bonus - pawns that control center squares
    uint64_t center = (1ULL << 27) | (1ULL << 28) | (1ULL << 35) | (1ULL << 36);
    uint64_t center_control = 0;
    
    // White pawns controlling center
    if (own_pawns & (1ULL << 19)) center_control |= 1ULL << 27; // d2 controls d4
    if (own_pawns & (1ULL << 20)) center_control |= 1ULL << 28; // e2 controls e4
    if (own_pawns & (1ULL << 27)) center_control |= 1ULL << 35; // d3 controls d5
    if (own_pawns & (1ULL << 28)) center_control |= 1ULL << 36; // e3 controls e5
    
    // Black pawns controlling center
    uint64_t opp_center_control = 0;
    if (opp_pawns & (1ULL << 44)) opp_center_control |= 1ULL << 36; // d6 controls d5
    if (opp_pawns & (1ULL << 45)) opp_center_control |= 1ULL << 35; // e6 controls e5
    if (opp_pawns & (1ULL << 35)) opp_center_control |= 1ULL << 27; // d4 controls d3
    if (opp_pawns & (1ULL << 36)) opp_center_control |= 1ULL << 26; // e4 controls e3
    
    int center_score = __builtin_popcountll(center_control) - __builtin_popcountll(opp_center_control);
    score += center_score * 5;
    
    // 2. Pawn break readiness
    // If you can break and opponent can't respond in kind
    int break_score = assessPawnBreakTiming(bd, color);
    score += break_score;
    
    // 3. Wing pawn breaks (a4, b4, h4, g4, etc.)
    // These are usually for creating weaknesses, not direct attacks
    
    // 4. The "break or be broken" assessment
    // If your pawn structure is static (blocked), you're in danger
    uint64_t own_blocked = own_pawns;
    // Pawn is blocked if pawn in front of it
    for (int f = 0; f < 8; f++) {
        for (int r = 1; r < 7; r++) {
            int sq = f + r * 8;
            if (own_pawns & (1ULL << sq)) {
                int block_sq = (color == WHITE) ? sq + 8 : sq - 8;
                if (block_sq >= 0 && block_sq < 64) {
                    if (opp_pawns & (1ULL << block_sq)) {
                        score -= 5; // Blocked pawn = weakness
                    }
                }
            }
        }
    }
    
    return score;
}

// ============================================================================
// CONVERSION MODE (AAGAARD)
// ============================================================================
//
// From Aagaard - Positional Play:
// Conversion Mode = When to switch from dynamic to static play
//
// Static advantages: Material, pawn structure, king safety
// Dynamic advantages: Initiative, piece activity, space
//
// If you have a CLEAR static advantage:
// → Switch to conversion mode (simplify, trade, convert)
// If you only have dynamic advantage:
// → Keep playing dynamically (don't simplify!)
//
// Returns bonus for correctly choosing conversion vs continuation
int evaluateConversionMode(const Board& bd, int color) {
    int score = 0;
    int opp = 1 - color;
    
    // Calculate advantage type
    int material_diff = 0;
    int pv[5] = {100, 320, 330, 500, 900};
    for (int pt = 0; pt < 5; pt++) {
        int w = __builtin_popcountll(bd.getPieces(WHITE, pt + 1));
        int bl = __builtin_popcountll(bd.getPieces(BLACK, pt + 1));
        material_diff += (w - bl) * pv[pt];
    }
    
    // Pawn structure advantage
    PawnStructure own_ps, opp_ps;
    analyzePawnStructure(bd, color, own_ps);
    analyzePawnStructure(bd, opp, opp_ps);
    int structure_diff = (own_ps.passed_count - opp_ps.passed_count) * 30 -
                        (own_ps.isolated_count - opp_ps.isolated_count) * 25;
    
    // Initiative
    int initiative = evaluateInitiative(bd, color);
    
    // Static advantage = material OR structure
    bool has_static_advantage = (material_diff > 150) || (structure_diff > 30);
    // Dynamic advantage = initiative
    bool has_dynamic_advantage = (initiative > 20);
    
    // Count major pieces
    uint64_t own_major = bd.getPieces(color, ROOKS) | bd.getPieces(color, QUEENS);
    uint64_t opp_major = bd.getPieces(opp, ROOKS) | bd.getPieces(opp, QUEENS);
    
    // In conversion mode, we want to:
    // 1. Trade into favorable endgames
    // 2. Simplify when ahead
    // 3. Play "quiet" moves
    
    if (has_static_advantage && !has_dynamic_advantage) {
        // We have a clear static advantage - should be converting
        // Bonus for having pieces that can trade down
        
        if (__builtin_popcountll(own_major) > __builtin_popcountll(opp_major)) {
            score += 20; // Can trade down to winning endgame
        }
        
        // Bonus for having passed pawns (easy to convert)
        if (own_ps.passed_count > 0) {
            score += own_ps.passed_count * 15;
        }
    }
    
    if (!has_static_advantage && has_dynamic_advantage) {
        // Only dynamic advantage - should NOT simplify
        score -= __builtin_popcountll(own_major) * 5; // Penalty for having many major pieces
    }
    
    return score;
}

bool detectExchangeSacrifice(const Board& bd, int color, int& discount) {
    int r = __builtin_popcountll(bd.getPieces(color, ROOKS));
    int or_ = __builtin_popcountll(bd.getPieces(1-color, ROOKS));
    int m = __builtin_popcountll(bd.getPieces(color, KNIGHTS)) + __builtin_popcountll(bd.getPieces(color, BISHOPS));
    int om = __builtin_popcountll(bd.getPieces(1-color, KNIGHTS)) + __builtin_popcountll(bd.getPieces(1-color, BISHOPS));
    if (r < or_ && m > om) { discount = (or_ - r) * 500 - (m - om) * 330; return true; }
    return false;
}

// ============================================================================
// MAIN EVALUATION
// ============================================================================

ImbalanceAnalysis analyzeImbalances(const Board& bd) {
    ImbalanceAnalysis ia = {};
    
    int pv[5] = {100, 320, 330, 500, 900};
    for (int pt = 0; pt < 5; pt++) {
        int w = __builtin_popcountll(bd.getPieces(WHITE, pt + 1));
        int bl = __builtin_popcountll(bd.getPieces(BLACK, pt + 1));
        ia.material += (w - bl) * pv[pt];
    }
    
    analyzePawnStructure(bd, WHITE, ia.white_pawns);
    analyzePawnStructure(bd, BLACK, ia.black_pawns);
    
    ia.pawn_structure += ia.white_pawns.passed_count * 30 - ia.black_pawns.passed_count * 30;
    ia.pawn_structure -= ia.white_pawns.isolated_count * 25 + ia.black_pawns.isolated_count * 25;
    ia.pawn_structure -= ia.white_pawns.backward_count * 20 + ia.black_pawns.backward_count * 20;
    ia.pawn_structure -= ia.white_pawns.doubled_count * 15 + ia.black_pawns.doubled_count * 15;
    ia.pawn_structure += (ia.black_pawns.island_count - ia.white_pawns.island_count) * 10;
    
    analyzePieceActivity(bd, WHITE, ia.white_activity);
    analyzePieceActivity(bd, BLACK, ia.black_activity);
    ia.activity = ia.white_activity.total_activity - ia.black_activity.total_activity;
    
    uint64_t wm = bd.getPieces(WHITE, PAWNS) | bd.getPieces(WHITE, KNIGHTS) | bd.getPieces(WHITE, BISHOPS) |
                  bd.getPieces(WHITE, ROOKS) | bd.getPieces(WHITE, QUEENS) | bd.getPieces(WHITE, KINGS);
    uint64_t bm = bd.getPieces(BLACK, PAWNS) | bd.getPieces(BLACK, KNIGHTS) | bd.getPieces(BLACK, BISHOPS) |
                  bd.getPieces(BLACK, ROOKS) | bd.getPieces(BLACK, QUEENS) | bd.getPieces(BLACK, KINGS);
    int ws = 0, bs = 0;
    for (int sq = 0; sq < 32; sq++) { if (wm & (1ULL << sq)) ws++; if (bm & (1ULL << (sq + 32))) bs++; }
    ia.space = (ws - bs) * 5;
    
    uint64_t wnp = bd.getPieces(WHITE, KNIGHTS) | bd.getPieces(WHITE, BISHOPS) | bd.getPieces(WHITE, ROOKS) | bd.getPieces(WHITE, QUEENS);
    uint64_t bnp = bd.getPieces(BLACK, KNIGHTS) | bd.getPieces(BLACK, BISHOPS) | bd.getPieces(BLACK, ROOKS) | bd.getPieces(BLACK, QUEENS);
    uint64_t wbr = (0xFFULL << 56) | 0xFFULL;
    uint64_t bbr = 0xFFULL;
    int wd = __builtin_popcountll(wnp & ~wbr);
    int bd_dev = __builtin_popcountll(bnp & ~bbr);
    ia.development = (wd - bd_dev) * 30;
    
    ia.initiative = (bd.getPlayerToMove() == WHITE) ? 10 : -10;
    ia.initiative += evaluateInitiative(bd, WHITE) - evaluateInitiative(bd, BLACK);
    
    ia.king_safety = evaluateKingSafety(bd, WHITE) - evaluateKingSafety(bd, BLACK);
    
    ia.white_king_exposed = !(bd.getCastlingRights() & WHITECASTLE) && bd.getKingSq(WHITE) >= 56;
    ia.black_king_exposed = !(bd.getCastlingRights() & BLACKCASTLE) && bd.getKingSq(BLACK) <= 7;
    ia.white_has_passed_pawn = ia.white_pawns.passed_count > 0;
    ia.black_has_passed_pawn = ia.black_pawns.passed_count > 0;
    ia.white_has_isolated = ia.white_pawns.isolated_count > 0;
    ia.black_has_isolated = ia.black_pawns.isolated_count > 0;
    ia.white_has_doubled = ia.white_pawns.doubled_count > 0;
    ia.black_has_doubled = ia.black_pawns.doubled_count > 0;
    
    if (detectExchangeSacrifice(bd, WHITE, ia.exchange_discount)) ia.exchange_sacrifice = true;
    if (detectExchangeSacrifice(bd, BLACK, ia.exchange_discount)) { ia.exchange_sacrifice = true; ia.exchange_discount = -ia.exchange_discount; }
    
    ia.minority_attack = detectMinorityAttack(bd, WHITE);
    ia.open_file = detectRookOnOpenFile(bd, WHITE);
    ia.rook_on_7th = detectRookOn7th(bd, WHITE);
    
    ia.is_endgame = ia.material < 2500;
    if (ia.is_endgame) {
        // Comprehensive endgame evaluation (Shereshevsky principles)
        int w_eg = evaluateEndgame(bd, WHITE);
        int b_eg = evaluateEndgame(bd, BLACK);
        
        ia.king_activity_white = w_eg;
        ia.king_activity_black = b_eg;
        ia.opposition_status = evaluateOpposition(bd, WHITE);
        
        // Add endgame bonus to pawn structure (king activity matters in endings)
        ia.pawn_structure += (w_eg - b_eg) / 5;
    }
    
    // Pawn storm detection (opposite castling)
    ia.opposite_castling = detectOppositeCastling(bd);
    if (ia.opposite_castling) {
        ia.pawn_storm = true;
        ia.pawn_storm_strength = evaluatePawnStorm(bd, WHITE) - evaluatePawnStorm(bd, BLACK);
        ia.king_safety_discount = 0;
        // King vulnerable to storm?
        if (isKingVulnerableToStorm(bd, WHITE)) ia.king_safety_discount -= 30;
        if (isKingVulnerableToStorm(bd, BLACK)) ia.king_safety_discount += 30;
    }
    
    calculatePositionalDiscounts(ia, current_style);
    
    return ia;
}

void calculatePositionalDiscounts(ImbalanceAnalysis& ia, PlayingStyle style) {
    switch (style) {
        case PlayingStyle::ATTACKING:
        case PlayingStyle::TACTICAL: ia.exchange_discount *= 2; ia.initiative_discount = 50; break;
        case PlayingStyle::POSITIONAL:
        case PlayingStyle::TECHNICAL: ia.exchange_discount /= 2; break;
        default: break;
    }
    if (ia.black_king_exposed) ia.king_safety_discount = 50;
    if (ia.white_king_exposed) ia.king_safety_discount = -50;
}

MoveExplanation explainMove(const Board& bd, int move, const ImbalanceAnalysis& ia) {
    MoveExplanation exp = {};
    std::stringstream ss;
    
    if (ia.material > 100) { ss << "Mat +" << ia.material/100 << ".0"; exp.imbalance_notes.push_back(ss.str()); }
    if (ia.material < -100) { ss << "Mat " << ia.material/100 << ".0"; exp.imbalance_notes.push_back(ss.str()); }
    if (ia.white_pawns.passed_count > 0) exp.imbalance_notes.push_back("Passed pawn");
    if (ia.black_pawns.passed_count > 0) exp.imbalance_notes.push_back("Opp passed pawn");
    if (ia.white_pawns.isolated_count > 0) exp.imbalance_notes.push_back("Isolani");
    if (ia.black_pawns.isolated_count > 0) exp.imbalance_notes.push_back("Opp isolani");
    if (ia.exchange_sacrifice) { ss.str(""); ss << "R for minor"; exp.sacrifice_notes.push_back(ss.str()); }
    if (ia.initiative > 15) { exp.imbalance_notes.push_back("Strong initiative"); exp.move_reasons.push_back("Maintain initiative"); }
    if (ia.white_king_exposed) { exp.imbalance_notes.push_back("King safety concern"); exp.move_reasons.push_back("Defend king"); }
    if (ia.black_king_exposed) { exp.imbalance_notes.push_back("Opp king exposed"); exp.move_reasons.push_back("Attack!"); }
    if (ia.minority_attack) exp.plan_notes.push_back("Minority attack");
    if (ia.open_file) exp.plan_notes.push_back("Open file");
    if (ia.rook_on_7th) exp.plan_notes.push_back("7th rank");
    if (ia.development > 60) exp.move_reasons.push_back("Better development");
    if (ia.is_endgame && ia.king_activity_white > ia.king_activity_black) exp.imbalance_notes.push_back("Active king");
    if (ia.opposition_status > 0) exp.plan_notes.push_back("Have opposition");
    if (ia.opposition_status < 0) exp.plan_notes.push_back("Opp has opposition");
    if (ia.opposite_castling) exp.plan_notes.push_back("Opposite castling");
    if (ia.pawn_storm) exp.plan_notes.push_back("Pawn storm");
    if (ia.king_safety_discount < -10) exp.imbalance_notes.push_back("King exposed to storm");
    if (ia.king_safety_discount > 10) exp.imbalance_notes.push_back("Opp king exposed to storm");
    
    exp.pv_explanation = "";
    auto add_vec = [&](const std::vector<std::string>& v) {
        for (size_t i = 0; i < v.size(); i++) {
            if (!exp.pv_explanation.empty()) exp.pv_explanation += " | ";
            exp.pv_explanation += v[i];
        }
    };
    add_vec(exp.sacrifice_notes); add_vec(exp.plan_notes); add_vec(exp.move_reasons); add_vec(exp.imbalance_notes);
    if (exp.pv_explanation.empty()) exp.pv_explanation = "Developing move";
    return exp;
}

int endgamePatienceBonus(const Board& bd, int color) {
    int mat = 0, pv[5] = {100, 320, 330, 500, 900};
    for (int pt = 0; pt < 5; pt++) { 
        mat += __builtin_popcountll(bd.getPieces(0, pt + 1)) * pv[pt]; 
        mat += __builtin_popcountll(bd.getPieces(1, pt + 1)) * pv[pt]; 
    }
    if (mat > 2500) return 0;
    int ks = bd.getKingSq(color), oks = bd.getKingSq(1-color);
    int d = abs((ks % 8) - 3) + abs((ks / 8) - 3);
    int od = abs((oks % 8) - 3) + abs((oks / 8) - 3);
    int bonus = (d < od) ? (od - d) * 5 : 0;
    int diff = abs((ks / 8) - (oks / 8)) + abs((ks % 8) - (oks % 8));
    if (diff > 0 && diff % 2 == 0) bonus += 15;
    return bonus;
}

int initiativeBonus(const Board& bd, int color) {
    int b = (bd.getPlayerToMove() == color) ? 10 : 0;
    b += __builtin_popcountll((bd.getPieces(color, KNIGHTS) | bd.getPieces(color, BISHOPS) | bd.getPieces(color, ROOKS) | bd.getPieces(color, QUEENS))) * 5;
    return b;
}

int prophylaxisBonus(const Board& bd, int color) {
    uint64_t opp = bd.getPieces(1-color, PAWNS);
    int fa = __builtin_popcountll(opp & 0x0101010101010101ULL);
    int fh = __builtin_popcountll(opp & 0x8080808080808080ULL);
    return (fa == 0 || fh == 0) ? 10 : 0;
}

int exchangeSacrificeValue(const Board& bd, int color) {
    int v = 0;
    int ks = bd.getKingSq(1-color);
    if ((bd.getPieces(color, ROOKS) & 0xFFULL)) v += 30;
    if ((1-color == WHITE && ks >= 56) || (1-color == BLACK && ks <= 7)) v += 50;
    return v;
}

int pawnSacrificeValue(const Board& bd, int color) {
    ImbalanceAnalysis ia = analyzeImbalances(bd);
    int v = 0;
    if (color == WHITE) { if (ia.development > 30) v += 20; if (ia.initiative > 0) v += 30; if (ia.space > 20) v += 20; }
    else { if (ia.development < -30) v += 20; if (ia.initiative < 0) v += 30; if (ia.space < -20) v += 20; }
    return v;
}

} // namespace HumanEval
