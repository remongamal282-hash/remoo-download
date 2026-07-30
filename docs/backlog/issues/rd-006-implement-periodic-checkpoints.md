---
title: "[RD-006] Implement periodic checkpoints"
labels: type:feature, module:storage, priority:must
milestone: v1.0
---

### Description
Save checkpoints every N seconds while downloads are active.

### SDS Reference
SDS-08 #6

### Acceptance Criteria
- [ ] Checkpoint interval is configurable.
- [ ] SQLite records are updated without blocking the UI.

### Dependencies
RD-007

### Estimate
3 story points
