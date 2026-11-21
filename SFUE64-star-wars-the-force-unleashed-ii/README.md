# Star Wars: The Force Unleashed II: GameCube controllers (work in progress)

For **SFUE64, the USA disc** (its `main.dol` has CRC32 `557329C6`). **Work in progress:** it covers the Multiplayer (duel) mode, up to four players on GameCube controllers (ports 1 to 4), each shown to the game as a Wii Remote with a Nunchuk. It hasn't been tried on a console yet.

## Layout

| GameCube controller | Menus | A match |
| --- | --- | --- |
| Control stick | Cursor | Move (tap jump if that port's option is on) |
| A | A | Attack |
| B | B (back) | Force power, held |
| Z | | Force power too |
| X / Y | | Jump |
| L / R | | Block, held |
| C-stick left / right | | The swing move (a quick sideways jolt) |
| Start | + | + |
| D-pad | D-pad | D-pad |

Each port has a "Tap jump" option (pushing the stick up jumps).

## Story Mode

In Story Mode the pack steps aside on any port where a real Wii Remote is connected, so Story plays as usual with a Wii Remote. A GameCube-controller-only port keeps the menu layout there (stick, A, B, Start), enough to get through Story's menus, not to play it.

## Setting it up

1. Copy `sd/` to the root of your SD card: `sd:/riivolution/swtfu2_pad.xml`.
2. Patch your own `main.dol` with `main.dol.bps` (see the main README) and put it at `sd:/swtfu2_pad/main.dol`.
3. In RiftWii: The Force Unleashed II > Mods > "GameCube controllers (Multiplayer, test)": Enabled, and the per-port tap jump options if wanted.

## Where it's tested

- **Dolphin:** the menus, Multiplayer's lobby (ports 1 to 4 join by themselves and each token moves with its own stick) and Story's menus, with the emulated adapter and with a GameCube controller in a Wii's own port.
- Not yet: a full match, a console.

## Known issues

- The special moves (the Saber Throw, a held B's charged power) haven't been seen working.
- In the Multiplayer lobby, A on "Back" and B do nothing.
- No rumble.

## How it works

The pack adds one section to `main.dol` with a `PADRead` (which RiftWii finds and fills from the adapter; on a Wii it reads the console's own ports) and points five of the game's calls into it: the per-frame `KPADRead` (a Wii Remote + Nunchuk state from the GameCube controller), two `WPADProbe` calls ("connected, Nunchuk", which Multiplayer requires) and the game's Wii Remote sync request, which it answers itself. Without that answer, a match stops with "Communications with the Wii Remote have been interrupted". See `src/`.
