---
title: "[RD-005] Implement fallback for servers without Range support"
labels: type:feature, module:engine, priority:must
milestone: v1.0
---

### Description
Automatically switch to single-stream download when Range requests are unsupported.

### SDS Reference
SDS-08 #5

### Acceptance Criteria
- [ ] HEAD/GET capability detection records Range support.
- [ ] Unsupported servers download with one connection.

### Dependencies
RD-001

### Estimate
3 story points
