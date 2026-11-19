# Changelog

## 1.0.0 - 2026-11-18 - working

### Added
- uLogLevel in NorseManifest.ini: 2 = info (the default), 0 = everything for a bug report.
- First release. It contains everything built and tested in 0.1.0, listed here and kept as history below:
- Pack manifests in ue4ss/NorsePacks: Name, Version, Author, Priority, Requires (>= version), Optional, Conflicts, GameBuild, Files.
- Requirement checks with plain reasons; one load order (requirements first, then Priority, then name); loose files last.
- include/NorseManifest.hpp for the frameworks; the NorseManifest mod writes the full report; console nm.list.
- main.dll carries a Windows version resource (file and product version 1.0.0, shown in its Properties > Details).

## 0.1.0 - 2026-11-17 (tested offline, 18 checks, and in game: TEST-RESULT-0.1.0.txt)
### Added
- Pack manifests in ue4ss/NorsePacks: Name, Version, Author, Priority, Requires (>= version), Optional, Conflicts, GameBuild, Files.
- Requirement checks with plain reasons; one load order (requirements first, then Priority, then name); loose files last.
- include/NorseManifest.hpp for the frameworks; the NorseManifest mod writes the full report; console nm.list.
