# Parametric Composition Foundation

Goal: add a reusable composition template surface that behaves like an AE-style precomp/effect preset hybrid, but stays deliberately narrow for safety.

`Slot` is the generic terminal that connects `ParametricComposition` to the outside world.
The first implementation only needs the input path, but the type model should already leave room for:

- `InputSlot`
- `OutputSlot`
- `ControlSlot`
- `EventSlot`

## Constraints

- Exactly 1 input slot
- Exactly 1 output
- Output format is RGBA only
- Cyclic references are rejected by input ancestry checks
- Unconnected input returns transparent RGBA
- Instance-level parameter overrides are allowed
- Cache key is `definitionId + input frame hash + parameter values + time`

## Scope

- Generic slot metadata
- Input binding model
- Public parameter override model
- Transparent fallback render path
- Deterministic cache key generation

## Non-goals

- Multi-input graph routing
- Multi-output fanout
- Automatic graph execution scheduling
- New UI wiring
