---
title: "[RD-003] Implement pause and resume"
labels: type:feature, module:core, priority:must
milestone: v1.0
---

### Description
Persist and restore download state safely.

### SDS Reference
SDS-08 #3

### Acceptance Criteria
- [ ] Pause stores segment progress.
- [ ] Resume continues from the last completed byte.
- [ ] Resume works after application restart.

### Dependencies
RD-002, RD-007

### Estimate
5 story points
