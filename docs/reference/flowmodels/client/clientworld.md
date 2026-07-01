# ClientWorld — the native client world (CLIENT-ONLY; the local build path)

Engine object: **ClientWorld** (`FClientWorld`, native). Client-only — there is no server counterpart
(the server uses `GWorld`; see `gworld.md §0`). Shares the OID→object table + `bff990` primitives with
GWorld (`gworld.md §1` map C).

## Methods (client-invoked)
| Lua / native | native path | does |
|---|---|---|
| `Connect` | `FUser::connect → FClientWorld::Connect → ca47e0 → bff990` | builds `TClient(Connector)` LOCALLY (MTLC), registers it in the OID table (`647860`). Trigger = GSS `0x1001` (`message-triggered-local-construction`, LATE-61..64) |
| `ClientWorld:CreateLocalObject(name)` | `c48760 → CreateDeferedTemplate → DefaultImpl` | builds a FULL registered instance locally (proven with `TPlayerDummy`; `tplayer-local-construct-late111`). Used for the char-select 3D preview |
| `ClientWorld:FindObject(oid)` | OID-table resolve | client analog of `GWorld:FindObject`; used in `Player.lua:518/1004/1029` |

## Edges
`clientworld ─▶ gworld (shares map C + bff990/647860)` · `CreateLocalObject ─▶ template (DefaultImpl)` ·
`Connect ─▶ session (drives sign-in)`.

## Note
`CreateLocalObject('TPlayer')` returns nil = TPlayer's template lands on `EmptyImpl` (never registered),
NOT a construct failure (`template.md`). The build code WORKS (TPlayerDummy proves it); the char needs
the replication trigger, not local injection (rejected as drift, LATE-111).
