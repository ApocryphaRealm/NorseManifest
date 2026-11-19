# NorseManifest

Pack manifests, requirement checks and one load order for the Norse frameworks (NORSE: Oath of Blood, UE4SS v4.0.0).
Every data pack - ours, the community's, and each framework itself - carries a small manifest: its name, version,
what it needs and at what version, an optional game build, and the config files it owns. Packs whose needs are not met
are switched off with a plain reason; the rest get one load order.

From the Norse overhaul plan (#4); modelled on Satisfactory Mod Loader's
dependency lists (version ranges, optional dependencies).

## A manifest

`ue4ss\NorsePacks\<anything>.ini`:

```ini
[Pack]
Name     = Tougher Outcasts
Version  = 1.2.0
Author   = someone
Priority = 100                                   ; lower loads first, so a higher one wins a shared field (default 0)
Requires = NorsePatcher >= 0.1.0, NorseDistributor
Optional = Bigger Brutes >= 1.0                  ; loads first when present; a warning when missing
Conflicts = Weaker Outcasts                      ; the later of the two is switched off
GameBuild = EEF010BD-0CB90000                    ; only on this game build
Files    = Mods/NorsePatcher/patches/tougher-outcasts.ini, Mods/NorseDistributor/distributions/tougher-outcasts.ini
```

Rules: a missing, switched-off or too-old requirement switches a pack off (and whatever needs it); a pack for another
game build is off; the same name twice keeps the highest version; a requirement loop switches its packs off.
Requirements (and present optional packs) load before their dependents; otherwise by Priority, then name. A config
file no manifest claims is a loose file and is read after every pack, so a player's own tweak wins.

## Parts

- `include/NorseManifest.hpp` - the library, header-only and game-free. NorsePatcher and NorseDistributor call
  `Manifest::Resolve()` and `Arrange()` on their own files, so nothing depends on which mod loads first.
- `src/main.cpp` - the UE4SS mod: the full report (every pack, its place or why it is off) in `NorseManifest.log` at
  game start; console `nm.list` writes it again.
- `tools/manifest_test.cpp` - the offline test: `xmake build manifest_test && xmake run manifest_test` (18 checks).
- `package/NorsePacks/_NorseManifest.ini` - its own manifest; each framework ships one the same way.
- `docs/test-packs/` - the packs of the in-game test (TEST-RESULT-0.1.0.txt).

## Licence

GPL-3.0-or-later (see LICENSE).
