# LEGO Star Wars III: The Clone Wars: GameCube controllers

For **SC4E64, the USA disc, revision 1** (its `main.dol` has CRC32 `E1BD8CC4`). Each GameCube controller shows up to the game as a Wii Remote with a Nunchuk, so two players can play with GameCube controllers only: port 1 is player 1, port 2 is player 2, and a second player joins with Start as usual.

## Layout

| GameCube controller | Wii controller | In the game |
| --- | --- | --- |
| Control stick, D-pad | Nunchuk stick | Move, menus |
| A | A | Jump, select |
| B | B | Attack, back |
| X | Z | Special (the Force, build, grapple) |
| Y | C | Tag (switch character) |
| L / R | 1 / 2 | Free Play |
| Start | + | Pause, "Press + to Start" |

The layout follows LEGO Star Wars II on the GameCube. Z and the C-stick are free.

## A real Wii Remote on the same player

If a Wii Remote is also on for a player, the GameCube controller acts as its Nunchuk: a press on either counts, and the Wii Remote keeps its own pointer, motion and HOME button. If that Wii Remote has its own Nunchuk plugged in, the GameCube controller steps aside. A Wii Remote switching off while the GameCube controller is still plugged in doesn't drop the player.

## Setting it up

1. Copy `sd/` to the root of your SD card: `sd:/riivolution/lsw3_pad.xml` and `sd:/lsw3_pad/gcpad.bin`.
2. Patch your own `main.dol` with `main.dol.bps` (see the main README) and put it at `sd:/lsw3_pad/main.dol`.
3. In RiftWii: LEGO Star Wars III > Mods > "GameCube controllers (WUP-028)": On. (On a Wii, the Riivolution app works too.)

## Where it's tested

- **Wii U console with the GameCube adapter, through RiftWii** (the version before slot sharing): 1 and 2 players; Reset from the HOME Menu keeps the GameCube controllers; the screen doesn't dim.
- **Dolphin:** 1 and 2 players with the emulated adapter, and with a GameCube controller in a Wii's own port; sharing a player with an emulated Wii Remote (both controllers' presses count, HOME opens the HOME Menu, a Nunchuk on the Wii Remote makes the GameCube controller step aside).

## Known issues

- Holding RiftWii's L+R+Z+Start for its HOME Menu also pauses the game underneath (the pack passes the held Start through as +).
- A Wii Remote switching off and on mid-game hasn't been tried on a console yet.
- With a Wii Remote + Nunchuk set up in Dolphin, the GameCube controller's presses at the title screen were also ignored before the Wii Remote's first press; not explained yet.

## How it works

The game has no GameCube controller code, so the pack adds a small `PADRead` in place of an unused function in `main.dol` (RiftWii finds it and fills it from the adapter; on a Wii it reads the console's own ports). The rest of the pack is a 2.6 KB blob at the bottom of the game's main memory: it wraps the game's four `KPADRead` calls (each player's Wii Remote + Nunchuk state, written from the GameCube controller every frame) and its `WPADProbe` calls ("connected, Nunchuk" for a port with a GameCube controller), calls the game's own connect callback with "connected" or "gone", and sits in front of that callback so a real Wii Remote going off doesn't drop a player. The state looks like a Wii Remote lying still (pointer valid, horizon, distance): without that, the title screen ignores every press. See `src/`.
