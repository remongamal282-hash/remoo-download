---
title: "[RD-002] Implement segmented downloads"
labels: type:feature, module:engine, priority:must
milestone: v1.0
---

### Description
Split files into byte ranges and download segments concurrently.

### SDS Reference
SDS-08 #2

### Acceptance Criteria
- [ ] Range requests are planned according to file size and max connections.
- [ ] Completed segments are merged into the final file.
- [ ] Integration test validates byte-identical output from local test server.

### Dependencies
RD-001

### Estimate
8 story points
