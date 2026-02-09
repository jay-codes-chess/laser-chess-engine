# Human Chess Engine - Based on Laser

A human-like chess engine focused on strategic thinking and teaching.

## Based on Laser by Jeffrey An and Michael An
- MIT Licensed
- ~7,400 lines of clean C++11
- Strong knowledge-based evaluation (~3,200 ELO)

## Our Extensions

### Phase 1: Teaching Features (Priority)
- [ ] Verbal PV explanations ("I play this because...")
- [ ] Imbalance analysis output (material, pawn structure, development, etc.)
- [ ] Style profiles affecting evaluation:
  - Classical (balanced)
  - Attacking (exaggerate initiative)
  - Positional (exaggerate pawn structure)
  - Technical (patient, "do not hurry")
  - Tactical (exaggerate tactics)

### Phase 2: Human-Like Evaluation
- [ ] Silman-style imbalance emphasis (more dramatic)
- [ ] Shereshevsky's "do not hurry" endgame principle
- [ ] Initiative bonuses for active pieces
- [ ] Exaggerated style profile effects

### Phase 3: Optional MCTS
- [ ] MCTS search mode for variety
- [ ] Self-play training for NNUE (future)

## Books Absorbed
- Silman - "How to Reassess Your Chess" (imbalances)
- Silman - "The Complete Endgame Course"
- Sokolov - "Sacrifice and Initiative in Chess"
- Shereshevsky - "Endgame Strategy"
- Shereshevsky - "Closed Endgames"
- Russian Chess School Vol 1 & 2
- Vukovic - "The Art of Attack"
- Kotov - "Play Like a Grandmaster"
- Marovic - "Dynamic Pawn Play in Chess"
- McDonald - "Positional Sacrifices"

## Building

### macOS/Linux
```bash
make
```

### Windows (MinGW)
```bash
make USE_STATIC=true
```

### Windows (MSVC)
```bash
# TODO: Add MSVC build
```

## License
Original Laser code: MIT
Our extensions: MIT
