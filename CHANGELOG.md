# Changelog

## 0.1.0 - 2026-11-17 (tested offline, 18 checks, and in game: TEST-RESULT-0.1.0.txt)
### Added
- Pack manifests in ue4ss/NorsePacks: Name, Version, Author, Priority, Requires (>= version), Optional, Conflicts, GameBuild, Files.
- Requirement checks with plain reasons; one load order (requirements first, then Priority, then name); loose files last.
- include/NorseManifest.hpp for the frameworks; the NorseManifest mod writes the full report; console nm.list.
