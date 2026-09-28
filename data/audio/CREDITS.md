# Audio credits

## Music

All three modules are by **Beyond**, released into the **public domain** (Creative Commons Public Domain), as stated on each module's page of The Mod Archive (checked 2026-09-28). The downloaded files are committed unchanged in `data/audio/`; only their names differ. `tools/itinst.py` writes the versions the ROM uses into `data/audio/snes/`, adapted to snesmod's driver, samples untouched:

- snesmod only plays modules in instrument mode: each sample gets an instrument mapping every note to it (*Offer the light* is already in instrument mode). When openmpt123 is installed, `tools/test_itinst.py` checks that this step renders the same.
- snesmod never starts a note that carries a tone portamento (G) on a silent channel, where Impulse Tracker starts it normally: such notes lose their G (the *Alonely* bass and chords).
- snesmod's timer makes every tick about 2% longer: the initial tempo and T commands are replaced by the tempo whose snesmod tick is closest to the original one.

| File | Title | Source |
|---|---|---|
| `alonely.it` | Alonely | https://modarchive.org/index.php?request=view_by_moduleid&query=172186 |
| `offerthelight.it` | Offer the light | https://modarchive.org/index.php?request=view_by_moduleid&query=213276 |
| `purity.it` | Purity | https://modarchive.org/index.php?request=view_by_moduleid&query=213554 |

## Sound effects

From **Digital Audio** by **Kenney** (https://kenney.nl/assets/digital-audio), licence **CC0 1.0** (http://creativecommons.org/publicdomain/zero/1.0/), downloaded 2026-09-28. Credit is not required; it is given here anyway.

| File | Original |
|---|---|
| `sfx/place.wav` | `pepSound1.ogg` |
| `sfx/refuse.wav` | `lowDown.ogg` |
| `sfx/undo.wav` | `phaserDown1.ogg` |
| `sfx/tick.wav` | `twoTone1.ogg` |
| `sfx/menu.wav` | `tone1.ogg` |
| `sfx/select.wav` | `highUp.ogg` |
| `sfx/win.wav` | `powerUp8.ogg` |

Each WAV was converted once with:

    ffmpeg -i <original>.ogg -ac 1 -ar 8000 -sample_fmt s16 <name>.wav

`sfx.it` is generated from these WAVs by `tools/mksfx.py`.
