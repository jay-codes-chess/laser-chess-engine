# Laser Man 🦸‍♂️
![](logos/laser_logo_small.png)

**A human-like chess engine focused on strategic thinking and teaching.**

Laser Man is based on Laser by Jeffrey An and Michael An, extended with GM-level chess knowledge for teaching students of all levels.

## Philosophy

**Knowledge > Calculation, Teaching > Strength**

Unlike traditional engines that rely on brute-force calculation, Laser Man thinks like a human Grandmaster:
- Evaluates positions using Silman-style imbalances
- Understands endgame principles (Shereshevsky: "Do not hurry")
- Uses prophylaxis (Russian School) to restrict opponent
- Knows when to convert vs when to keep attacking (Aagaard)
- Appreciates initiative and tempo (Euwe/Kramer)

## Playing Styles

Laser Man has **5 personalities** that affect move selection:

| Style | Philosophy |
|-------|------------|
| **Classical** | Balanced engine play |
| **Attacking** | Initiative > Material |
| **Tactical** | Sacrifices welcome, aggressive |
| **Positional** | Patient, structure-focused |
| **Technical** | Strong endgame, "do not hurry" |

## UCI Options

Dev options for teaching:
- `Style` - Choose personality
- `EvalMaterial` (0-200) - How much material matters
- `EvalPawnStructure` (0-200) - Pawn weakness emphasis
- `EvalSpace` (0-200) - Center control
- `EvalDevelopment` (0-200) - Piece activity
- `EvalInitiative` (0-200) - Tempo/attack priority
- `EvalKingSafety` (0-200) - King safety emphasis
- `ShowEval` - Display evaluation info
- `ShowExplanation` - Verbal move explanations

## Usage

Laser Man is a UCI engine. Use with any chess GUI:
- [Arena](http://www.playwitharena.com)
- [Cute Chess](http://www.cutechess.com)
- ChessBase

## Credits

- **Base Engine**: Laser by Jeffrey An and Michael An
- **Teaching Concepts**: Silman, Shereshevsky, Euwe/Kramer, Aagaard, Russian School
- [Chess Programming Wiki](https://www.chessprogramming.org)

## Build

```bash
# macOS
bash build-macos.sh

# Windows (MinGW)
build-windows.bat
```

## Version

- **Laser Man 1.0 TEACH** - Based on Laser 1.7
- Built with human strategic evaluation
