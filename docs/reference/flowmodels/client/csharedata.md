# CSharedData (client) — OwnerOnly Com of TAccount (game data, client-local)

Engine object: **CSharedData** (`Class/Main/CSharedData.lua`; an OwnerOnly Com of `TAccount`). Server
counterpart: `server/src/account/`.

## Client handshake
| Lua | file:line | does |
|---|---|---|
| `LuaOnClientLoad` | `CSharedData.lua:16` | sets `DataTable[k]` from `self.m_Data = GWorld.m_SharedData` — **CLIENT-LOCAL**, NOT from our wire |

## Key fact (LATE-140/141 — retracts an earlier claim)
CSharedData sourcing from `GWorld.m_SharedData` means **empty wire content is CORRECT-for-phase**: the
game-data (DataTable/PortalGraph) arrives later in-game from the client's own GWorld, not from the
account-tier wire. This is NOT a fidelity gap (`taccount-fully-faithful-late140`). Of TAccount's 4 Coms,
only CSharedData has a `LuaOnClientLoad`, and it's GWorld-sourced ⇒ **no account-tier handshake blocks
char load.**

## Edges
`csharedata ─▶ gworld (m_SharedData)` · sibling Coms in `taccount.md`.
