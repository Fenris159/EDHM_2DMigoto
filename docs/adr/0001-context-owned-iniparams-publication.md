---
status: proposed
---

# Keep pending IniParams publication with its context

The preliminary branch retains pending publication on each context and shares
checked upload handling across commands, transitions and initialization. A
successful upload recorded on a deferred context does not acknowledge work on
the immediate context, so a global or device-wide published generation would
change visibility semantics. Preserve existing publication points and request
frequency first; evaluate redundant-upload suppression separately after native
context-ordering and workload measurements. See the
[implementation status](../pre-production-edo-readiness.md) for validation limits.
