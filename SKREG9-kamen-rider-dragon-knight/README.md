# Kamen Rider: Dragon Knight: GameCube controllers

For **SKREG9, the USA disc** (every patch checks the game's original code first, so it does nothing on another version). Each GameCube controller shows up to the game as a Classic Controller, which the game already supports, so it only has to be told one is plugged in. Port 1 is player 1, port 2 player 2.

## Layout

In menus: A confirms, B goes back, the D-pad and control stick move. In a battle, the recommended layout (each part is an option in RiftWii's Mods menu or Riivolution):

| GameCube controller | In a battle |
| --- | --- |
| Control stick | Move |
| B | Light attack |
| A | Heavy attack |
| L | Jump |
| R | Dash |
| Y | Cancel |
| Z | Rider Art (card) |
| C-stick | Style actions (the Classic D-pad) |
| D-pad up | Finale + Charge |
| Start | Pause |
| Start + Z | HOME Menu |

The options:
- **Face buttons:** Layout A (B light, A heavy) or Layout B (A light, B heavy).
- **Jump:** R jumps and L dashes, Y jumps and L dashes, or L jumps and R dashes.
- **Extra buttons:** style actions on the C-stick or on the D-pad.
- **HOME button:** Start + Z, or off.

The game's own button pictures stay the Classic Controller's.

## Setting it up

1. Copy `sd/` to the root of your SD card: `sd:/riivolution/krdk_gcpad.xml` and `sd:/krdk_gcpad/gcpad.bin`. No `main.dol` is needed.
2. In RiftWii: Kamen Rider: Dragon Knight > Mods > "GameCube controllers (WUP-028)": On, and pick the options. (On a Wii, the Riivolution app works too.)

## Where it's tested

- **Wii U console with the GameCube adapter, through RiftWii:** 1 and 2 players; Reset from the HOME Menu keeps the GameCube controllers.
- **Dolphin:** 1 and 2 players with the emulated adapter, and with a GameCube controller in a Wii's own port.

## How it works

A 3.8 KB blob at the bottom of the game's main memory wraps the game's `KPADRead` calls and the HOME Menu's `WPADProbe` call: for a port with a GameCube controller, the game's Classic Controller fields are filled from it (its own Classic support does the rest), with a menu layout outside battles and the battle layout in them. It's a port of my Kamen Rider: Super Climax Heroes pack (same developer, the same controller code a few months older). See `src/`.
