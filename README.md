# GameCube controllers in Wii games

Riivolution packs that let a GameCube controller play Wii games that only take Wii controllers. Each pack makes the GameCube controller show up to the game as the Wii controller it expects (a Classic Controller, or a Wii Remote with a Nunchuk), with a layout for that game. They work with a Wii's own GameCube ports, and on a Wii U with the Wii U GameCube adapter through [RiftWii](https://github.com/KakarottoCake/Riftwii), which feeds the pack from the adapter.

| Game | Disc | The GameCube controller shows up as | Players | Status |
| --- | --- | --- | --- | --- |
| [LEGO Star Wars III: The Clone Wars](SC4E64-lego-star-wars-iii) | SC4E64 (USA, Rev 1) | Wii Remote + Nunchuk | 1-2 | Works (Wii U console and Dolphin) |
| [Kamen Rider: Dragon Knight](SKREG9-kamen-rider-dragon-knight) | SKREG9 (USA) | Classic Controller | 1-2 | Works (Wii U console and Dolphin) |
| [Batman: The Brave and the Bold – The Videogame](S3BEWR-batman-the-brave-and-the-bold) | S3BEWR (USA) | Wii Remote + Nunchuk, with a pointer and shakes | 1-2 | Works in Dolphin; not yet tried on a console |
| [Star Wars: The Force Unleashed II](SFUE64-star-wars-the-force-unleashed-ii) | SFUE64 (USA) | Wii Remote + Nunchuk | 1-4 | **Work in progress:** Multiplayer only, special moves unconfirmed, not yet tried on a console |

GameCube port 1 is player 1, port 2 player 2, and so on. With no Wii Remote needed, these games play with GameCube controllers only.

## Using a pack

Each game's folder has an `sd` folder: copy what's in it to the root of your SD card, so you end up with `sd:/riivolution/<pack>.xml` and `sd:/<pack folder>/`.

Three packs (LEGO Star Wars III, Batman, The Force Unleashed II) also need a patched `main.dol`, the game's own program file, which can't be shared here. You make it from your own disc:

1. Get `main.dol` from your copy of the game. In Dolphin: right-click the game, Properties, Filesystem, then right-click the game's partition and extract `sys/main.dol`.
2. Apply the game's `main.dol.bps` to it, either with `python tools/bps.py apply main.dol main.dol.bps patched.dol`, or with a BPS patcher such as [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/). The patch checks your file first: a different region or revision is refused, with no harm done.
3. Put the result on the SD card as `sd:/<pack folder>/main.dol` (the game's folder README says which).

Then turn the pack on:
- **RiftWii (Wii U, or a Wii):** pick the game, open Mods, and switch on its GameCube controllers option.
- **Riivolution (a Wii):** start the game from the Riivolution app with the pack's option on.

## Building from source

Each game's `src` folder has the pack's code (C and PowerPC assembly) and a `build.py` that builds what's in `sd` (and the patched `main.dol`). You need Python 3 and devkitPPC (from devkitPro), and the game's own `main.dol`: set `MAIN_DOL` to its path, or put it next to `build.py`. The output goes to a `build` folder next to `src`. The scripts are written for Windows (they call the devkitPPC tools with `.exe`).

## How they work

Every pack does the same few things: it gives the game a GameCube controller read (`PADRead`) where the game has none, in the shape RiftWii looks for ([KakarottoCake/Riftwii#381](https://github.com/KakarottoCake/Riftwii/pull/381)), so on a Wii U RiftWii fills it from the adapter and on a Wii it reads the console's own ports; it tells the game a Wii controller is connected on that player's slot, and gone when the GameCube controller is unplugged; and every frame it writes that controller's state from the GameCube controller, with the game's layout. Each game's README has the details.

I've proposed doing the part that's the same in every game once, in RiftWii itself, so packs could shrink to just their layout.

## Thanks

To quatric, whose [Wii-Patches](https://github.com/Wii-Patches) and [porting guide](https://gist.github.com/quatric/257a40993345c2c7568b9154c00186fd) showed the way for much of this, and to Kakarotto Cake and RiftWii's contributors for RiftWii and its support for packs like these.

Not affiliated with or endorsed by Nintendo or the games' publishers. You need your own copy of each game. MIT licensed (see `LICENSE`).
