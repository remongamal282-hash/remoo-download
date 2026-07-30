---
title: "[RD-004] Implement auto reconnect"
labels: type:feature, module:engine, priority:must
milestone: v1.0
---

### Description
Use exponential backoff when network requests fail.

### SDS Reference
SDS-08 #4

### Acceptance Criteria
- [ ] Transient failures enter retrying/reconnecting state.
- [ ] Retry delay grows with attempt count.
- [ ] Max retry exhaustion marks the download failed once.

### Dependencies
RD-001

### Estimate
5 story points
