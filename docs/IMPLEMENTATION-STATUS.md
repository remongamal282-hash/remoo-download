# Implementation Status for SDS 01-09

This file maps the SDS chapters to repository artifacts. It is a
practical audit document: each row points to the files that currently implement
or operationalize the chapter.

| SDS | Scope | Implementation Status | Primary Artifacts |
|---|---|---|---|
| SDS-01 | Vision, scope, license, target platforms | Implemented as project metadata and documentation | `README.md`, `LICENSE`, `CMakeLists.txt`, `vcpkg.json` |
| SDS-02 | IDM feature analysis and redesigned feature set | Partially implemented as staged product capabilities plus backlog items for deferred work | `include/`, `src/`, `extensions/`, `docs/backlog/backlog.yml` |
| SDS-03 | Architecture | Implemented as separated core, GUI, and native-host build targets | `CMakeLists.txt`, `src/browser/native_host_main.cpp`, `include/`, `src/` |
| SDS-04 | SQLite database design | Implemented by migration source and runtime `StorageManager` migration | `db/migrations/0001_init.sql`, `include/storage/`, `src/storage/` |
| SDS-05 | UI/UX, RTL, themes, dialogs | Implemented as Qt Widgets UI, RTL default, QSS themes, and translation sources | `src/ui/`, `include/ui/`, `resources/themes/`, `resources/translations/` |
| SDS-06 | UML diagrams | Implemented as Mermaid source files and documentation | `docs/06-UML-Diagrams.md`, `docs/diagrams/*.mmd` |
| SDS-07 | Code standards, tests, CI/CD | Implemented as format/tidy configs, test target, fixtures, and GitHub Actions | `.clang-format`, `.clang-tidy`, `.pre-commit-config.yaml`, `tests/`, `.github/workflows/ci.yml` |
| SDS-08 | Backlog and GitHub issues | Implemented as machine-readable backlog, templates, labels, milestones, and generated issue files | `docs/backlog/backlog.yml`, `docs/backlog/issues/`, `.github/ISSUE_TEMPLATE/`, `.github/labels.yml`, `.github/milestones.yml`, `tools/export-backlog-issues.ps1` |
| SDS-09 | Release roadmap | In progress for the v0.1 execution path; later releases remain future work | `all-fileMD/09-Release-Roadmap.md`, `docs/09-Release-Plan.md`, `src/cli/main.cpp`, `CMakeLists.txt` |

## Completed During This Audit

- Added missing `LICENSE` and `CODE_OF_CONDUCT.md`.
- Added `.github/milestones.yml`.
- Added `tools/export-backlog-issues.ps1` and generated 59 issue files.
- Synchronized `db/migrations/0001_init.sql` with the runtime migration in
  `StorageManager`.
- Added `resources/Info.plist` for the macOS bundle target.
- Added Chromium and Firefox extension scaffolds with Native Messaging support.
- Added Native Messaging manifests under `resources/native-messaging/`.
- Added Qt translation source files for Arabic and English.
- Split the CMake build into `remo_core`, `RemooDownload`, and
  `remo_native_host` targets.
- Replaced the unused external `spdlog` dependency in logging code with a
  standard-library logger.
- Added a v0.1 CLI entry point (`remo_cli`) and made GUI/native-host targets
  optional via `BUILD_GUI`.
- Added `INetworkClient`, `CurlNetworkClient`, and `MockNetworkClient` so the
  download engine can be tested without real network access.
- Added `SegmentPlanner` and wired `DownloadEngine` to it for deterministic
  initial segment planning and fallback to a single segment when byte ranges are
  unsupported.
- Updated `DownloadEngine` to download planned segments concurrently up to the
  configured connection limit, then merge completed part files in order.
- Added unit tests for segment planning and mocked segmented download/merge.

## Verification Notes

- `tools/export-backlog-issues.ps1` currently exports 59 issue files.
- `git diff --check` passes for the current working tree.
- CMake is available only through the vcpkg downloads cache. A CLI-only
  configure attempt reached compiler detection but failed because no C++
  compiler / `nmake` is available on `PATH`.

## Boundaries

SDS-08 is a backlog document. Its generated issues include v1.x and v2.0+
features that are intentionally future work, not mandatory implementation for
the current v0.1 source tree.
