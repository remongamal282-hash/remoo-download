---
title: "[RD-001] Implement DownloadEngine using libcurl multi"
labels: type:feature, module:engine, priority:must
milestone: v1.0
---

### Description
Central download engine using the libcurl multi interface.

### SDS Reference
SDS-08 #1

### Acceptance Criteria
- [ ] DownloadEngine can start, pause, resume, and cancel tracked downloads.
- [ ] Engine reports progress and active transfer counts.
- [ ] Unit tests cover successful start and cancellation paths.

### Dependencies
None

### Estimate
5 story points
