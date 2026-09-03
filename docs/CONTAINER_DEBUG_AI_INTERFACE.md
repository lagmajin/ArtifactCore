# Container Debug / AI Interface

**最終更新:** 2026-09-01

## Purpose

Custom containers can expose a bounded diagnostic snapshot to AI and debugging
clients without exposing their elements, pointers, or mutation APIs. The first
supported container is `NamedVector<T>`.

Other custom containers that provide `debugSnapshot()` can use the shared
`registerContainerDebugSnapshot(registry, id, container)` helper. They receive
the same read-only snapshot surface; note writing requires an explicit writer
registration such as the `NamedVector` convenience API.

## Owner contract

An owner must explicitly register a container. Registration is opt-in and is
not automatic.

```cpp
ArtifactCore::NamedVector<int> selectedIds{
  ArtifactCore::ContainerName{"SelectionIds"}};

auto selectedIdsDebugRegistration =
  selectedIds.registerDebugSnapshot("selection.activeIds");
```

Keep the returned `ContainerDebugRegistry::Registration` alive for as long as
the container is exposed. Destroy the registration before the container. Its
destructor removes the reader and waits for an in-flight registry inspection to
finish.

## Read-only AI surface

The MCP tool `debug.containers` returns only containers registered in
`ContainerDebugRegistry::instance()`. Each entry has an ID, an availability
flag, and a structured snapshot containing container metadata, counters,
element samples, and debug notes.
It accepts an optional `id` argument to return only one registered instance;
when omitted, all registered instances are returned.

`debug.containers.annotate` can append a bounded debug note to a registered
`NamedVector`. The tool fixes the note author to `AI`; it cannot edit elements,
clear a container, change capacity, or perform rollback. A domain owner must
provide a dedicated, validated operation before an AI can change application
state. It requires a non-empty ID and text; allowed severities are `info`,
`warning`, `error`, and `hypothesis`.
When the write succeeds, the response also includes the stored note's
timestamp and observed version when the snapshot is still available.

Snapshot readers are isolated at the registry boundary. If a reader raises an
exception, the entry is reported as unavailable and the other registered
containers can still be returned. The exception text is not forwarded to the
AI response.

## Debug notes

`NamedVector::addDebugNote()` records an event with timestamp, severity,
author, source location, and observed version.

- Maximum history: 32 notes per container
- Maximum note body: 1024 bytes
- Note authors: `Runtime`, `Developer`, `AI`
- Note severities: `Info`, `Warning`, `Error`, `Hypothesis`

The method returns `false` for empty or oversized text.

Severity and author values outside the enums are rejected as well. The text
diagnostic formatter exposes sample indices and notes, but never element
addresses; this is also true of the JSON formatter.

## Property Live Patch boundary

Property Live Patch sessions are serialized and identified by a session token
returned from `debug.patch.begin`. The token is required for
`debug.patch.apply`, `debug.patch.rollback`, and `debug.patch.commit`; a second
begin is rejected while a session is active. This prevents another MCP client
from silently replacing or completing an in-progress patch session.

## Checkpoints

`createDebugCheckpoint()` captures a value copy of a `NamedVector<T>`. It is
intended only for copyable value types and debug-time recovery.

`rollbackToDebugCheckpoint()` requires the caller's expected current version.
It fails if the checkpoint belongs to another container or if the container has
changed since that version was observed. This prevents an AI recovery attempt
from silently overwriting another execution path.

## Concurrency boundary

`ContainerDebugRegistry` synchronizes registration, lookup, and deregistration.
It does not synchronize the registered container's own mutation or snapshot
generation. Owners of concurrently mutated containers must provide an
appropriately synchronized snapshot reader.
