# Audio credits

## Music

All three modules are by **Beyond**, released into the **public domain** (Creative Commons Public Domain), as stated on each module's page of The Mod Archive (checked 2026-09-28). The files are committed unchanged; only their names differ.

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
