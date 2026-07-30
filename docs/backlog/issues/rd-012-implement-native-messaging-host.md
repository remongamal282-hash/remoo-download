---
title: "[RD-012] Implement Native Messaging Host"
labels: type:feature, module:browser, priority:must
milestone: v1.0
---

### Description
Receive links from browsers through stdin/stdout native messaging.

### SDS Reference
SDS-08 #12

### Acceptance Criteria
- [ ] Host reads length-prefixed JSON messages.
- [ ] Invalid messages are rejected safely.
- [ ] Valid addDownload messages are forwarded to the app/service layer.

### Dependencies
None

### Estimate
5 story points
