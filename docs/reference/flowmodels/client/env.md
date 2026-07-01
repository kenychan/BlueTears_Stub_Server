# Environment / NObject accessors — the pervasive native globals

The two most-called native accessors in the flow Lua (72× each across the account/char files) plus the
NID hash. Server counterpart: `server/src/api/` (`host_context.py` binds these for the host Lua VM).

## Globals
| global | role |
|---|---|
| `GetEnvironment()` | fetch the engine environment/config singleton (game constants, tables) |
| `GetNObject(...)` | fetch/resolve an NObject by handle/name (the generic engine-object accessor) |
| `GetNID(name)` | def-table NID hash (native; also in `template.md`) |

These are infrastructure — nearly every Lua method touches `GetEnvironment`/`GetNObject`. They are
client+server (bound on whichever VM runs). The host already binds `GetEnvironment`/`GetNObject` in
`server/src/api/host_context.py` (M-API, GREEN) so the shipped server Lua runs.

## Edges
Called by nearly every object file; no distinct flow of its own — the ambient engine accessor layer.

## OPEN
Full enumeration of `GetNObject` handle keys is not needed for E1–E3; revisit if a specific env lookup
gates a later milestone.
