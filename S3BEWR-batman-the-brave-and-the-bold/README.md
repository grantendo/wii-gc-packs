# Batman: The Brave and the Bold – The Videogame: GameCube controllers

For **S3BEWR, the USA disc** (its `main.dol` has CRC32 `EA4C0FD6`). Each GameCube controller shows up to the game as a Wii Remote with a Nunchuk, with a pointer and shakes, so two players can play with GameCube controllers only: port 1 is player 1, port 2 player 2.

## Layout

| GameCube controller | Wii controller | In the game |
| --- | --- | --- |
| Control stick | Nunchuk stick | Move |
| C-stick | Pointer | Menus |
| A | A | Punch |
| B | B | Jump |
| L | C | Block |
| R | − | Grapple |
| Z | Z | Gadget |
| D-pad left / right | D-pad | Change gadget |
| D-pad up | 1 | Jump-In Hero |
| Start | + | Pause |
| Y (held) | Shake the Wii Remote | Heavy attack |
| X (held) | Shake both | Power attack |

## Setting it up

1. Copy `sd/` to the root of your SD card: `sd:/riivolution/batman_pad.xml`.
2. Patch your own `main.dol` with `main.dol.bps` (see the main README) and put it at `sd:/batman_pad/main.dol`.
3. In RiftWii: Batman > Mods > "GameCube controllers (WUP-028)": On. (On a Wii, the Riivolution app works too.)

## Where it's tested

- **Dolphin:** 1 and 2 players with the emulated adapter, and with a GameCube controller in a Wii's own port.
- Not yet tried on a console.

## How it works

The pack adds one section to `main.dol` with a `PADRead` (which RiftWii finds and fills from the adapter; on a Wii it reads the console's own ports) and the code that turns each GameCube controller into a Wii Remote + Nunchuk state every frame, for the game's `KPADRead` calls and `WPADProbe` calls. A shake is a hard enough jolt on all three axes, so holding Y or X jolts the controller every few frames; the game counts a shake past 3.2 g. The game moves its menu pointer by how far the pointer moves, so a C-stick pointer stays where it's left. See `src/`.
